#ifndef TRIPLE_COMPARISON_HPP
#define TRIPLE_COMPARISON_HPP

#include <list>
#include <string>
#include <tuple>
#include <set>

 struct TripleIDHasher
  {
    size_t
    operator()(const hdt::TripleID & obj) const
    {
    	std::stringstream ss;
    	ss << obj.getSubject() << ' ' << obj.getPredicate() << ' ' << obj.getObject();
      return std::hash<std::string>()(ss.str());
    }
  };

  struct TripleIDComparator
  {
    bool
    operator()(const hdt::TripleID& obj1, const hdt::TripleID & obj2) const
    {
    	// Subject comparison
		if (obj1.getSubject() != obj2.getSubject()) {
			return false;
		}

		// Object comparison (since subject was successful)
		if (obj1.getObject() != obj2.getObject()) {
			return false;
		}

		// Predicate comparison (since subject and object were successful)
		if (obj1.getPredicate()!= obj2.getPredicate()) {
			return false;
		}
		return true;
    }
  };

#endif /* TRIPLE_COMPARISON_HPP */
