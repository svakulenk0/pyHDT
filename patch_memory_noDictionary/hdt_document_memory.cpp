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
#include <algorithm>
#include <pybind11/stl.h>

#include "../hdt-cpp-1.3.2/libhdt/src/triples/TriplesList.hpp"
#include "../hdt-cpp-1.3.2/libhdt/src/triples/TriplesComparator.hpp"

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
  if (file!=""){
	  hdt_file = file;
	  if (!file_exists(file)) {
	    throw std::runtime_error("Cannot open HDT file '" + file + "': Not Found!");
	  }
	  //hdt = HDTManager::mapIndexedHDT(file.c_str());
	  hdt = HDTManager::loadIndexedHDT(file.c_str());
	  processor = new QueryProcessor(hdt);
  }

  numHops=1;
  filterPrefixStr="";
  continuousDictionary=true;
  typeString="http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
  preffixIniSO=0;
  preffixEndSO=0;
  preffixIniSUBJECT=0;
  preffixEndSUBJECT=0;
  preffixIniOBJECT=0;
  preffixEndOBJECT=0;
  literalEndID=0;
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

string HDTDocument::globalIdToString (unsigned int id, hdt::TripleComponentRole role){
	if (role==OBJECT){
		if (continuousDictionary && id>hdt->getDictionary()->getNsubjects()){
			// convert the id to the traditional one
			id = id - (hdt->getDictionary()->getNsubjects()-hdt->getDictionary()->getNshared());
		}
	}

	return hdt->getDictionary()->idToString(id,role);

}

unsigned int HDTDocument::StringToGlobalId (string term, hdt::TripleComponentRole role){
	unsigned int id = hdt->getDictionary()->stringToId(term,role);
		if (role==OBJECT){
			if (continuousDictionary && id>hdt->getDictionary()->getNsubjects()){
				id=id+(hdt->getDictionary()->getNsubjects()-hdt->getDictionary()->getNshared());
			}
		}
		return id;
}

void HDTDocument::configureHops(int setnumHops,vector<unsigned int> filterPredicates,string setfilterPrefixStr,bool setcontinuousDictionary, bool setincludeLiterals){
	numHops = setnumHops;
	preds.clear();
	std::copy(filterPredicates.begin(),
			filterPredicates.end(),
	            std::inserter(preds, preds.end()));
	continuousDictionary = setcontinuousDictionary;

	// Get range of preffix
	filterPrefixStr = setfilterPrefixStr;

	// get the ID of literals if needed
	if (setfilterPrefixStr=="" || setfilterPrefixStr!="predef-dbpedia2016-04" || setfilterPrefixStr!="predef-wikidata2020-03-all"){
		IteratorUCharString * itObjects = hdt->getDictionary()->getObjects();
		bool foundNonString=false;
		literalEndID = hdt->getDictionary()->getNshared();
		while (itObjects->hasNext() && !foundNonString){
			literalEndID++;
			unsigned char * str = itObjects->next();
			if (strncmp((const char *)str,(const char *)"\"",1)!=0){
				foundNonString=true;
			}
		}
	}


	if (setfilterPrefixStr!=""){
		if (setfilterPrefixStr=="predef-dbpedia2016-04"){ // FOR DBPEDIA 2016-04
			preffixIniSO=2979755;
			preffixEndSO=24597521;
			preffixIniOBJECT=151243949;
			preffixEndOBJECT=153168015;
			preffixIniSUBJECT=50097212;
			preffixEndSUBJECT=52750736;
			literalEndID=147777579;
		}
		else if (setfilterPrefixStr=="predef-wikidata2020-03-all"){
			literalEndID=1924886681;
		}
		else{
			IteratorUInt *itIDSol = hdt->getDictionary()->getIDSuggestions(setfilterPrefixStr.c_str(),SUBJECT);
			unsigned int sol=0,prev=0,ini=0;

			bool soZone=true;
			while (itIDSol->hasNext()) {
				sol =  itIDSol->next();
				//cout << "solution ID is "<<sol << ", which corresponds to string: "<<hdt->getDictionary()->idToString(sol,SUBJECT)<<endl;
				if (ini==0) {
					if (sol<=hdt->getDictionary()->getNshared())
						preffixIniSO=sol;
					else{
						preffixIniSUBJECT=sol;
						soZone=false;
					}
					ini++;
				}
				else{
					if (soZone && sol >hdt->getDictionary()->getNshared()){
						preffixEndSO=prev;
						preffixIniSUBJECT=sol;
						soZone=false;
					}

				}
				prev=sol;
			}
			if (soZone)
				preffixEndSO=sol;
			else
				preffixEndSUBJECT=sol;
			/*
				cout << "First solution ID SO is "<<preffixIniSO << ", which corresponds to string: "<<hdt->getDictionary()->idToString(preffixIniSO,SUBJECT)<<endl;
				cout << "Last solution ID  SO is "<<preffixEndSO << ", which corresponds to string: "<<hdt->getDictionary()->idToString(preffixEndSO,SUBJECT)<<endl;
				cout << "First solution ID SUBJECT  is "<<preffixIniSUBJECT << ", which corresponds to string: "<<hdt->getDictionary()->idToString(preffixIniSUBJECT,SUBJECT)<<endl;
				cout << "Last solution ID SUBJECT  is "<<preffixEndSUBJECT << ", which corresponds to string: "<<hdt->getDictionary()->idToString(preffixEndSUBJECT,SUBJECT)<<endl;
			*/
			itIDSol = hdt->getDictionary()->getIDSuggestions(setfilterPrefixStr.c_str(),OBJECT);
			ini=0,prev=0,sol=0;
			bool OZone=false;
			while (itIDSol->hasNext()) {
				sol =  itIDSol->next();
				if (!OZone && sol>preffixEndSO){
					preffixIniOBJECT=sol;
					OZone=true;
				}
			}
			if (OZone)
				preffixEndOBJECT=sol;
			/*
				cout << "First solution ID OBJECT  is "<<preffixIniOBJECT << ", which corresponds to string: "<<hdt->getDictionary()->idToString(preffixIniOBJECT,OBJECT)<<endl;
				cout << "Last solution ID OBJECT  is "<<preffixEndOBJECT << ", which corresponds to string: "<<hdt->getDictionary()->idToString(preffixEndOBJECT,OBJECT)<<endl;

			*/
		}
	}

}

/*!
* Compute the reachable triples from the given terms, in the configure number of numHops.
* @param terms
* @param classes
*/

vector<vector<unsigned int>> HDTDocument::filterTypeIDs(vector<unsigned int> terms,vector<unsigned int> classes){

	map<unsigned int, vector<unsigned int>> classesToEntities; //for each class, the entities with this class
	vector<unsigned int> classesCorrectId; //the provided classes with the correct IDs

	//initialize map with the classes provided
	for (int i=0;i<classes.size();i++){
		vector<unsigned int> entities;
		unsigned int classID=classes[i];
		if (continuousDictionary){ //get the appropriate ID
			if (classID>hdt->getDictionary()->getNsubjects()){
				// convert the id to the traditional one
				classID = classID - (hdt->getDictionary()->getNsubjects()-hdt->getDictionary()->getNshared());
			}
		}
		classesCorrectId.push_back(classID);
		classesToEntities[classID]=entities;
	}

	// get the ID of the type
	unsigned int typeID = hdt->getDictionary()->stringToId(typeString,PREDICATE);
	for (int i=0;i<terms.size();i++){
		unsigned int term =terms[i];
		IteratorTripleID *it=NULL;
		TripleID patternSubject(term,typeID,0);
		it  = hdt->getTriples()->search(patternSubject);
		while (it->hasNext())
		{
			unsigned int classValueID = it->next()->getObject();
			if (classesToEntities.find(classValueID)!=classesToEntities.end()){ //if the class is one of the one we are interested, add the entity
				classesToEntities[classValueID].push_back(term);
			}

		}
		delete it;
	}

	vector<vector<unsigned int>> ret;
	//prepare output
	for (int i=0;i<classesCorrectId.size();i++){
	//for(std::map<unsigned int, vector<unsigned int>>::iterator iter = classesToEntities.begin(); iter != classesToEntities.end(); ++iter)
		//unsigned int classID =  iter->first;
		ret.push_back(classesToEntities[classesCorrectId[i]]);
	}
	return ret;

}

std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> HDTDocument::computeAllHopsIDs(vector<unsigned int> terms){
	return computeHopsIDs(terms,hdt->getTriples()->getNumberOfElements(),0);
}

std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> HDTDocument::computeHopsIDs(vector<unsigned int> terms, unsigned int limit, unsigned int offset){
	processedTerms.clear();
	processedTriples=0;
	readTriples=0;
	skippedtriplesSet.clear();
	outtriplesSet.clear();
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
					addhop(term,1,role,limit,offset);
				}
			}
			else{
				// with the traditional dictionary, it could be ambiguous as we need the rol for the non shared subjects and objects. Thus, by default we will consider both
				addhop(term,1,SUBJECT,limit,offset);
				addhop(term,1,OBJECT,limit,offset);
			}


		}
	}
	processedTerms.clear();
	return outputMatrix();
}


std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> HDTDocument::outputMatrix(){


	//sort PSO and remove duplicates
	TripleComponentOrder order = PSO;
	
	std::vector<TripleID> ordered(outtriplesSet.begin(), outtriplesSet.end());
	std::sort(ordered.begin(), ordered.end(), TriplesComparator(order));
	//prepare output matrix
	vector<vector<std::tuple<unsigned int, unsigned int>>> matrix;

	// dump output
	unsigned int prevPredicate=0;
	vector<unsigned int> predicates;

	vector<std::tuple<unsigned int, unsigned int>> currentPredicateMatrix;
	map<unsigned int, unsigned int> mappingGlobalToLocalID; //mapping to keep the global to id order
	vector<unsigned int> mappingLocalToGlobalID;

	//while (it->hasNext())
	for (auto iter = ordered.begin(); iter != ordered.end(); ++iter)
	{
		//TripleID *triple = it->next();
		TripleID triple = *iter;
		if (triple.getPredicate()!=prevPredicate){
			// save previous vector
			if (currentPredicateMatrix.size()>0){
				matrix.push_back(currentPredicateMatrix);
			}
			currentPredicateMatrix.clear();
			// store the current one
			predicates.push_back(triple.getPredicate());
			prevPredicate=triple.getPredicate();
		}

		unsigned int subject = triple.getSubject();
		unsigned int object = triple.getObject();
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
	ordered.clear();
	skippedtriplesSet.clear();
	outtriplesSet.clear();
	mappingGlobalToLocalID.clear();

	std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> ret =std::make_tuple(mappingLocalToGlobalID,predicates,matrix);
	return ret;
}

void HDTDocument::addhop(size_t termID,int currenthop,TripleComponentRole role, unsigned int limit, unsigned int offset){

	if (processedTriples<limit){ // check if we exceed the limit in terms of number of triples
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
						//if (filterPrefixStr=="" || (hdt->getDictionary()->idToString(triple->getObject(),OBJECT).find(filterPrefixStr) != std::string::npos)){
						if (filterPrefixStr=="" || (includeLiterals==true && triple->getObject()<literalEndID) || ((triple->getObject()>=preffixIniSO) && (triple->getObject() <=preffixEndSO)) || ((triple->getObject()>=preffixIniOBJECT) && (triple->getObject() <=preffixEndOBJECT))){
							if (processedTriples<limit){ // check if we exceed the limit in terms of number of triples
								if (readTriples<offset){ //check if we need to skip some offset
									if (skippedtriplesSet.find(*triple)==skippedtriplesSet.end()){ //only count as skipped if the triple is not skipped before
										readTriples++;
										skippedtriplesSet.insert(*triple); //mark as skipped
									}
								}
								else{
									// only insert as a solution if the triple has not been skipped (sometimes there are repetitions)
									if (skippedtriplesSet.find(*triple)==skippedtriplesSet.end())
										outtriplesSet.insert(*triple);
								}
								processedTriples=outtriplesSet.size(); // keep the count of the triples for the potential limit
								if ((currenthop+1)<=numHops){ // we could do it in the beginning of the function but it saves time to do it here and avoid to change the context
									if (processedTerms.find(triple->getObject())==processedTerms.end()){
										//if (verbose) cout<<"next hop object"<<endl;
										addhop(triple->getObject(),currenthop+1,OBJECT,limit,offset);
									}
								}
							}
						}
					}

				}
				delete it;
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
					// For shared SO, skip the special case in which subject=object as it is already done as subject
					if (termID>hdt->getDictionary()->getNshared() || (triple->getSubject()!=triple->getObject())){
						// check the predicate filter if needed
						if (preds.size()==0 || preds.find(triple->getPredicate())!=preds.end())
						{
							//check the prefix if needed
							//if (filterPrefixStr=="" || (hdt->getDictionary()->idToString(triple->getObject(),OBJECT).find(filterPrefixStr) != std::string::npos)){
							if (filterPrefixStr=="" || (includeLiterals==true && triple->getObject()<literalEndID) || ((triple->getObject()>=preffixIniSO) && (triple->getSubject() <=preffixEndSO)) || ((triple->getObject()>=preffixIniSUBJECT) && (triple->getSubject() <=preffixEndSUBJECT))){
								if (processedTriples<limit){ // check if we exceed the limit in terms of number of triples
									if (readTriples<offset){ //check if we need to skip some offset
										if (skippedtriplesSet.find(*triple)==skippedtriplesSet.end()){ //only count as skipped if the triple is not present before
											readTriples++;
											skippedtriplesSet.insert(*triple);
										}
									}
									else{
										// only insert as a solution if the triple has not been skipped (sometimes there are repetitions)
										if (skippedtriplesSet.find(*triple)==skippedtriplesSet.end())
											outtriplesSet.insert(*triple);
									}
									processedTriples=outtriplesSet.size(); // keep the count of the triples for the potential limit
									if ((currenthop+1)<=numHops){ // we could do it in the beginning of the function but it saves time to do it here and avoid to change the context
										if (processedTerms.find(triple->getSubject())==processedTerms.end()){
											// if (verbose)cout<<"next hop subject"<<endl;
											addhop(triple->getSubject(),currenthop+1,SUBJECT,limit,offset);
										}
									}
								}
							}
						}
					}
				}
				delete it;
			}
		}
	//	delete it;
	}
}
void HDTDocument::remove(){
	delete hdt;
}

void HDTDocument::setHDT(hdt::HDT* hdtCopy){
	hdt = hdtCopy;
        processor = new QueryProcessor(hdt);
}

hdt::HDT* HDTDocument::getHDT(){
	return hdt;
}
void HDTDocument::cloneHDT (HDTDocument doc){
	hdt = doc.getHDT();
	processor = new QueryProcessor(hdt);
}
