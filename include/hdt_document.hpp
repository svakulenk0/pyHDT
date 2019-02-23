/**
 * hdt_document.hpp
 * Author: Thomas MINIER - MIT License 2017-2018
 */

#ifndef PYHDT_DOCUMENT_HPP
#define PYHDT_DOCUMENT_HPP

#include "HDT.hpp"
#include "QueryProcessor.hpp"
#include "pyhdt_types.hpp"
#include "triple_iterator.hpp"
#include "triple_comparison.hpp"
#include "tripleid_iterator.hpp"
#include "join_iterator.hpp"
#include <list>
#include <string>
#include <vector>

// new inclusions
#include <HDTEnums.hpp>
#include <Triples.hpp>
#include <unordered_set>
#include <tuple>

// The result of a search for a triple pattern in a HDT document:
// a tuple (matching RDF triples, nb of matching RDF triples)
typedef std::tuple<TripleIterator *, size_t> search_results;

// Same as seach_results, but for an iterator over triple ids
typedef std::tuple<TripleIDIterator *, size_t> search_results_ids;

/*!
 * HDTDocument is the main entry to manage an hdt document
 * \author Thomas Minier
 */
class HDTDocument {
private:
  std::string hdt_file;
  hdt::HDT *hdt;
  hdt::QueryProcessor *processor;
  HDTDocument(std::string file);

/*!
   * Add a new hop starting from the given termID
   * @param termID
   * @param currenthop
   * @param role
   * @param limit
   * @param offset
   */
void addhop(size_t termID,int currenthop,hdt::TripleComponentRole role,unsigned int limit, unsigned int offset);

  /*!
   * Output the result of the hop, in outtriples
   */
  std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> outputMatrix();

  int numHops;
  string filterPrefixStr;
  bool continuousDictionary;
  std::unordered_set<unsigned int> preds;
  std::unordered_set<size_t> processedTerms;
  unsigned int processedTriples;
  unsigned int readTriples;
  string typeString;
  unsigned int preffixIniSO;
  unsigned int preffixEndSO;
  unsigned int preffixIniSUBJECT;
  unsigned int preffixEndSUBJECT;
  unsigned int preffixIniOBJECT;
  unsigned int preffixEndOBJECT;

  // Declaring unordered_set of TripleID
   std::unordered_set<hdt::TripleID, TripleIDHasher,TripleIDComparator> outtriplesSet;
   std::unordered_set<hdt::TripleID, TripleIDHasher,TripleIDComparator> skippedtriplesSet;

public:
  /*!
   * Destructor
   */
  ~HDTDocument();

  /*!
   * Get the path to the HDT file currently loaded
   * @return [description]
   */
  std::string getFilePath();

  /*!
   * Implementation for Python function "__repr__"
   * @return [description]
   */
  std::string python_repr();

  /*!
   * Get the total number of triples in the HDT document
   * @return [description]
   */
  unsigned int getNbTriples();

  /*!
   * Get the number of subjects in the HDT document
   * @return [description]
   */
  unsigned int getNbSubjects();

  /*!
   * Get the number of predicates in the HDT document
   * @return [description]
   */
  unsigned int getNbPredicates();

  /*!
   * Get the number of objects in the HDT document
   * @return [description]
   */
  unsigned int getNbObjects();

  /*!
   * Get the number of shared subjects-objects in the HDT document
   * @return [description]
   */
  unsigned int getNbShared();

  /*!
   * Static factory method used to create a new HDT Document
   * @param  file
   */
  static HDTDocument create(std::string file) { return HDTDocument(file); }

  /*!
   * Convert a TripleID to a string triple pattern
   * @param  subject   [description]
   * @param  predicate [description]
   * @param  object    [description]
   * @return           [description]
   */
  triple idsToString(unsigned int subject, unsigned int predicate,
                     unsigned int object);

  /*!
   * Search all matching triples for a triple pattern, whith an optional limit
   * and offset. Returns a tuple<TripleIterator*, cardinality>
   * @param subject   [description]
   * @param predicate [description]
   * @param object    [description]
   * @param limit     [description]
   * @param offset    [description]
   */
  search_results search(std::string subject, std::string predicate,
                        std::string object, unsigned int limit = 0,
                        unsigned int offset = 0);

  /*!
   * Same as search, but for an iterator over TripleIDs.
   * Returns a tuple<TripleIDIterator*, cardinality>
   * @param subject   [description]
   * @param predicate [description]
   * @param object    [description]
   * @param limit     [description]
   * @param offset    [description]
   */
  search_results_ids searchIDs(std::string subject, std::string predicate,
                               std::string object, unsigned int limit = 0,
                               unsigned int offset = 0);

  JoinIterator * searchJoin(std::vector<triple> patterns);


 /*!
   * Configure the hop functionality
   * @param setnumHops number of hops (default 1)
   * @param filterPredicates predicates to consider in the hops, set "" for all
   * @param setfilterPrefixStr only consider entities with the given prefix, set "" for all
   * @param setcontinuousDictionary Output the result using a continuous mapping (object IDs after subjects) instead of the traditional HDT dictionary (default true)
   */
  void configureHops(int setnumHops,vector<unsigned int> filterPredicates,string setfilterPrefixStr,bool setcontinuousDictionary);

  /*!
   * Compute the reachable triples from the given terms, in the configure number of numHops.
   * @param terms
   */
  std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> computeAllHopsIDs(vector<unsigned int> terms);

  /*!
     * Compute the reachable triples from the given terms, in the configure number of numHops. It also sets the limit and offset in terms of number of triples
     * @param terms
     */
    std::tuple<vector<unsigned int>,vector<unsigned int>,vector<vector<std::tuple<unsigned int, unsigned int>>>> computeHopsIDs(vector<unsigned int> terms, unsigned int limit, unsigned int offset);

   /*!
     * Compute the reachable triples from the given terms, in the configure number of numHops.
     * @param terms
     * @param classes
     */
    vector<vector<unsigned int>> filterTypeIDs(vector<unsigned int> terms,vector<unsigned int> classes);

  /*!
   * Get the string associated to a given id in the dictionary
   * @param id
   * @param role
   * @return
   */
  string idToString (unsigned int id, hdt::TripleComponentRole role);

  /*!
     * Get the string associated to a given id in the dictionary
     * @param term
     * @param role
     * @return
     */
  unsigned int StringToid (string term, hdt::TripleComponentRole role);

  void remove();

};

#endif /* PYHDT_DOCUMENT_HPP */
