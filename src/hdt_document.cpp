/**
 * hdt_document.hpp
 * Author: Thomas MINIER - MIT License 2017-2018
 */

#include "hdt_document.hpp"
#include "triple_iterator.hpp"
#include <HDTEnums.hpp>
#include <HDTManager.hpp>
#include <SingleTriple.hpp>
#include <fstream>
#include <pybind11/stl.h>

#include "../hdt-cpp-1.3.2/libhdt/src/triples/TriplesList.hpp"

using namespace hdt;

/*!
 * Skip `offset` items from an iterator, optimized for HDT iterators.
 * @param it          [description]
 * @param offset      [description]
 * @param cardinality [description]
 */
template <typename T>
inline void applyOffset(T *it, unsigned int offset, unsigned int cardinality) {
  if (offset > 0 && offset >= cardinality) {
    // hdt does not allow to skip past beyond the estimated nb of results,
    // so we may have a few results to skip manually
    unsigned int remainingSteps = offset - cardinality + 1;
    it->skip(cardinality - 1);
    while (it->hasNext() && remainingSteps > 0) {
      it->next();
      remainingSteps--;
    }
  } else if (offset > 0) {
    it->skip(offset);
  }
}

/*!
 * returns true if a file is readable, False otherwise
 * @param  name [description]
 * @return      [description]
 */
inline bool file_exists(const std::string &name) {
  std::ifstream f(name.c_str());
  bool result = f.good();
  f.close();
  return result;
}

/*!
 * Constructor
 * @param file [description]
 */
HDTDocument::HDTDocument(std::string file) {
  hdt_file = file;
  if (!file_exists(file)) {
    throw std::runtime_error("Cannot open HDT file '" + file + "': Not Found!");
  }
  hdt = HDTManager::mapIndexedHDT(file.c_str());
  processor = new QueryProcessor(hdt);

  outtriples = new TriplesList();
  numHops=1;
  filterPrefixStr="";
  continuousDictionary=true;
}

/*!
 * Destructor
 */
HDTDocument::~HDTDocument() {}

/*!
 * Get the path to the HDT file currently loaded
 * @return [description]
 */
std::string HDTDocument::getFilePath() { return hdt_file; }

/*!
 * Implementation for Python function "__repr__"
 * @return [description]
 */
std::string HDTDocument::python_repr() {
  return "<HDTDocument " + hdt_file + " (~" + std::to_string(getNbTriples()) +
         " RDF triples)>";
}

/*!
 * Search all matching triples for a triple pattern, whith an optional limit and
 * offset. Returns a tuple<vector<triples>, cardinality>
 * @param subject   [description]
 * @param predicate [description]
 * @param object    [description]
 * @param limit     [description]
 * @param offset    [description]
 */
search_results HDTDocument::search(std::string subject,
                                   std::string predicate,
                                   std::string object,
                                   unsigned int limit,
                                   unsigned int offset) {
  search_results_ids tRes = searchIDs(subject, predicate, object, limit, offset);
  TripleIterator *resultIterator = new TripleIterator(std::get<0>(tRes), hdt->getDictionary());
  return std::make_tuple(resultIterator, std::get<1>(tRes));
}

/*!
 * Same as search, but for an iterator over TripleIDs.
 * Returns a tuple<TripleIDIterator*, cardinality>
 * @param subject   [description]
 * @param predicate [description]
 * @param object    [description]
 * @param limit     [description]
 * @param offset    [description]
 */
search_results_ids HDTDocument::searchIDs(std::string subject,
                                          std::string predicate,
                                          std::string object,
                                          unsigned int limit,
                                          unsigned int offset) {
  TripleID tp(hdt->getDictionary()->stringToId(subject, hdt::SUBJECT),
              hdt->getDictionary()->stringToId(predicate, hdt::PREDICATE),
              hdt->getDictionary()->stringToId(object, hdt::OBJECT));
  IteratorTripleID *it = hdt->getTriples()->search(tp);
  size_t cardinality = it->estimatedNumResults();
  // apply offset
  applyOffset<IteratorTripleID>(it, offset, cardinality);
  TripleIDIterator *resultIterator =
      new TripleIDIterator(it, subject, predicate, object, limit, offset);
  return std::make_tuple(resultIterator, cardinality);
}

/*!
 * Get the total number of triples in the HDT document
 * @return [description]
 */
unsigned int HDTDocument::getNbTriples() {
  return hdt->getTriples()->getNumberOfElements();
}

/*!
 * Get the number of subjects in the HDT document
 * @return [description]
 */
unsigned int HDTDocument::getNbSubjects() {
  return hdt->getDictionary()->getNsubjects();
}

/*!
 * Get the number of predicates in the HDT document
 * @return [description]
 */
unsigned int HDTDocument::getNbPredicates() {
  return hdt->getDictionary()->getNpredicates();
}

/*!
 * Get the number of objects in the HDT document
 * @return [description]
 */
unsigned int HDTDocument::getNbObjects() {
  return hdt->getDictionary()->getNobjects();
}

/*!
 * Get the number of shared subjects-objects in the HDT document
 * @return [description]
 */
unsigned int HDTDocument::getNbShared() {
  return hdt->getDictionary()->getNshared();
}

/*!
 * Convert a TripleID to a string triple pattern
 * @param  subject   [description]
 * @param  predicate [description]
 * @param  object    [description]
 * @return           [description]
 */
triple HDTDocument::idsToString(unsigned int subject, unsigned int predicate,
                                unsigned int object) {
  return std::make_tuple(
      hdt->getDictionary()->idToString(subject, hdt::SUBJECT),
      hdt->getDictionary()->idToString(predicate, hdt::PREDICATE),
      hdt->getDictionary()->idToString(object, hdt::OBJECT));
}

JoinIterator * HDTDocument::searchJoin(std::vector<triple> patterns) {
  set<string> vars {};
  vector<TripleString> joinPatterns {};
  std::string subj, pred, obj;

  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    // unpack pattern
    std::tie(subj, pred, obj) = *it;
    // add variables
    if (subj.at(0) == '?') {
      vars.insert(subj);
    }
    if (pred.at(0) == '?') {
      vars.insert(pred);
    }
    if (obj.at(0) == '?') {
      vars.insert(obj);
    }
    // build join pattern
    TripleString pattern(subj, pred, obj);
    joinPatterns.push_back(pattern);
  }

  VarBindingString *iterator = processor->searchJoin(joinPatterns, vars);
  return new JoinIterator(iterator);
}

string HDTDocument::idToString (unsigned int id, hdt::TripleComponentRole role){
	return hdt->getDictionary()->idToString(id,role);
}

unsigned int HDTDocument::StringToid (string term, hdt::TripleComponentRole role){
	return hdt->getDictionary()->stringToId(term,role);
}

void HDTDocument::configureHops(int setnumHops,vector<string> filterPredicates,string setfilterPrefixStr,bool setcontinuousDictionary){
	numHops = setnumHops;

	// convert the vector of strings in filter predicates to a vector of ids
	for (int i=0;i<filterPredicates.size();i++){
		unsigned int idPred = hdt->getDictionary()->stringToId(filterPredicates[i],PREDICATE);
		preds.insert(idPred);

	}
	filterPrefixStr = setfilterPrefixStr;
	continuousDictionary = setcontinuousDictionary;

}

std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> HDTDocument::computeHopsIDs(vector<unsigned int> terms){
	processedTerms.clear();
	outtriples= new TriplesList();
	// do a recursive function to iterate terms 2 hops, and keep the result in a TripleList, then order by PSO and dump.
	if (numHops>=1){
		TripleComponentRole role=SUBJECT;
		for (int i=0;i<terms.size();i++){
			unsigned int term =terms[i];
			if (continuousDictionary){
				if (term>hdt->getDictionary()->getNsubjects()){
					role=OBJECT;
					// convert the id to the traditional one
					term = term - (hdt->getDictionary()->getNsubjects()-hdt->getDictionary()->getNshared());
				}
				if (term!=0){
					addhop(term,1,role);
				}
			}
			else{
				// with the traditional dictionary, it could be ambiguous as we need the rol for the non shared subjects and objects. Thus, by default we will consider both
				addhop(term,1,SUBJECT);
				addhop(term,1,OBJECT);
			}


		}
	}
	processedTerms.clear();
	return outputMatrix();
}


std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> HDTDocument::outputMatrix(){


	//sort PSO and remove duplicates
	TripleComponentOrder order = PSO;
	outtriples->sort(order,NULL);
	outtriples->removeDuplicates(NULL);

	//prepare output matrix
	vector<vector<std::tuple<unsigned int, unsigned int>>> matrix;

	// dump output
	IteratorTripleID *it = outtriples->searchAll();
	unsigned int prevPredicate=0;
	vector<unsigned int> predicates;

	vector<std::tuple<unsigned int, unsigned int>> currentPredicateMatrix;
	map<unsigned int, unsigned int> mappingGlobalToLocalID; //mapping to keep the global to id order
	vector<unsigned int> mappingLocalToGlobalID;

	while (it->hasNext())
	{
		TripleID *triple = it->next();
		if (triple->getPredicate()!=prevPredicate){
			// save previous vector
			if (currentPredicateMatrix.size()>0){
				matrix.push_back(currentPredicateMatrix);
			}
			currentPredicateMatrix.clear();
			// store the current one
			predicates.push_back(triple->getPredicate());
			prevPredicate=triple->getPredicate();
		}

		unsigned int subject = triple->getSubject();
		unsigned int object = triple->getObject();
		if (continuousDictionary){// change the id of the object to make it continuous
			if (object>hdt->getDictionary()->getNshared()){
				object=object+(hdt->getDictionary()->getNsubjects()-hdt->getDictionary()->getNshared());
			}
		}

		//update the local id mappings
		if (mappingGlobalToLocalID.find(subject)==mappingGlobalToLocalID.end()){ //new entity
			mappingGlobalToLocalID[subject]=mappingLocalToGlobalID.size(); //keep new mapping, starting in 0
			mappingLocalToGlobalID.push_back(subject);
		}
		if (mappingGlobalToLocalID.find(object)==mappingGlobalToLocalID.end()){ //new entity
			mappingGlobalToLocalID[object]=mappingLocalToGlobalID.size(); //keep new mapping, starting in 0
			mappingLocalToGlobalID.push_back(object);
		}
		// insert tuple with the local mappings;
		std::tuple<unsigned int, unsigned int> pair = std::make_tuple(mappingGlobalToLocalID.find(subject)->second,mappingGlobalToLocalID.find(object)->second);
		currentPredicateMatrix.push_back(pair);
	}
	// insert the last row
	if (currentPredicateMatrix.size()>0){
		matrix.push_back(currentPredicateMatrix);
	}
	delete it;
	delete outtriples;
	mappingGlobalToLocalID.clear();

	std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> ret =std::make_tuple(mappingLocalToGlobalID,predicates,matrix);
	return ret;
}

void HDTDocument::addhop(size_t termID,int currenthop,TripleComponentRole role){

	processedTerms.insert(termID);
	IteratorTripleID *it=NULL;
	// process as a subjectID
	if (role==SUBJECT || termID<=hdt->getDictionary()->getNshared()){
		if (termID<=hdt->getDictionary()->getMaxSubjectID()){
			TripleID patternSubject(termID,0,0);
			//if (verbose) cout<< "searching termID "<<termID<<":"<<hdt->getDictionary()->idToString(termID,SUBJECT)<< endl;

			it  = hdt->getTriples()->search(patternSubject);
			while (it->hasNext())
			{
				TripleID *triple = it->next();

				// check the predicate filter if needed
				if (preds.size()==0 || preds.find(triple->getPredicate())!=preds.end())
				{
					//check the prefix if needed
					if (filterPrefixStr=="" || (hdt->getDictionary()->idToString(triple->getObject(),OBJECT).find(filterPrefixStr) != std::string::npos)){
						outtriples->insert(*triple);
						if ((currenthop+1)<=numHops){ // we could do it in the beginning of the function but it saves time to do it here and avoid to change the context
							if (processedTerms.find(triple->getObject())==processedTerms.end()){
								//if (verbose) cout<<"next hop object"<<endl;
								addhop(triple->getObject(),currenthop+1,OBJECT);
							}
						}
					}
				}

			}
		}
	}
	// process as a objectID
	if (role==OBJECT || termID<=hdt->getDictionary()->getNshared()){
		if (termID<=hdt->getDictionary()->getMaxObjectID()){
			TripleID patternObject(0,0,termID);
			// if (verbose) cout<< "searching termID "<<termID<<":"<<hdt->getDictionary()->idToString(termID,OBJECT)<< endl;
			it = hdt->getTriples()->search(patternObject);
			while (it->hasNext())
			{
				TripleID *triple = it->next();
				// check the predicate filter if needed
				if (preds.size()==0 || preds.find(triple->getPredicate())!=preds.end())
				{
					//check the prefix if needed
					if (filterPrefixStr=="" || (hdt->getDictionary()->idToString(triple->getObject(),OBJECT).find(filterPrefixStr) != std::string::npos)){
						outtriples->insert(*triple);
						if ((currenthop+1)<=numHops){ // we could do it in the beginning of the function but it saves time to do it here and avoid to change the context
							if (processedTerms.find(triple->getSubject())==processedTerms.end()){
								// if (verbose) cout<<"next hop subject"<<endl;
								addhop(triple->getSubject(),currenthop+1,SUBJECT);
							}
						}
					}
				}
			}
		}
	}
	delete it;
}
