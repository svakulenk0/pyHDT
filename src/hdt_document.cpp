/**
 * hdt_document.hpp
 * Author: Thomas MINIER - MIT License 2017-2018
 */

#include "hdt_document.hpp"
#include "HDTManager.hpp"

using namespace hdt;

/*!
 * Constructor
 * @param file [description]
 */
HDTDocument::HDTDocument(std::string file) {
  hdt_file = file;
  hdt = HDTManager::mapIndexedHDT(file.c_str());
}

/*!
 * Destructor
 */
HDTDocument::~HDTDocument() {}

/*!
 * Get the path to the HDT file currently loaded
 * @return [description]
 */
std::string HDTDocument::getFilePath() {
  return hdt_file;
}

/*!
 * Implementation for Python function "__repr__"
 * @return [description]
 */
std::string HDTDocument::python_repr() {
  return "<HDTDocument:" + hdt_file + ">";
}

/*!
 * Search all matching triples for a triple pattern, whith an optional limit and offset.
 * Returns a tuple<vector<triples>, cardinality>
 * @param subject   [description]
 * @param predicate [description]
 * @param object    [description]
 * @param limit     [description]
 * @param offset    [description]
 */
search_results HDTDocument::search(std::string subject, std::string predicate, std::string object, unsigned int limit, unsigned int offset) {
  std::list<triple> results;
  bool noLimit = limit == 0;
  // Search for triples
  IteratorTripleString *it = hdt->search(subject.c_str(), predicate.c_str(), object.c_str());

  size_t cardinality = it->estimatedNumResults();

  // apply offset
  if (offset > 0) {
    it->skip(offset);
  }

  // Gather results
  while(it->hasNext() && (noLimit || limit > results.size())) {
    TripleString *ts = it->next();
    triple t = std::make_tuple(ts->getSubject(), ts->getPredicate(), ts->getObject());
    results.push_back(t);
  }

  delete it;
  return std::make_tuple(results, cardinality);
}