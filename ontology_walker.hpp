//------------------------------------------------------------------------------
// Copyright 2011 Bryan St. Amour
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// This file is part of Raptor++.
//
// Raptor++ is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Raptor++ is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Raptor++.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

//===========================================================================
// This file defines the ontology_walker function object. The ontology
// walker crawls over an ontology (or any other collection of connected
// rdf documents) by following uri's that appear as objects of the triples.
// Each time a node is visited, a user-supplied function type is called on
// the node.
//===========================================================================

#ifndef BST_ONTOLOGY_WALKER_HPP_
#define BST_ONTOLOGY_WALKER_HPP_

#include "rdf_parser.hpp"

#include <list>
#include <set>
#include <string>
#include <utility>
#include <iostream>
#include <unordered_set>
#include <queue>

namespace rdf {

//===========================================================================
// This function object walks an ontology, applying a function-type at each
// node it encounters. Typical of graph walking algorithms, it keeps a
// closed list of nodes (in this case uri's) that have already been visited.
//===========================================================================
template <typename Function, typename Predicate>
class ontology_walker
{
  Function func_;
  Predicate pred_;

public:
  ontology_walker(Function&& func, Predicate&& pred)
    : func_(std::forward<Function>(func)), pred_(std::forward<Predicate>(pred))
  {}

  //----------------------------------------------------------------------
  // Given a uri string as a starting point, walk the graph created by
  // the rdf documents. Each time a URI appears as the object of a
  // triple, attempt to parse it and contine walking. The walking order
  // is breadth-first-search (it uses a queue) but this may be changed
  // via a tag class or something later on...
  //----------------------------------------------------------------------
  void operator()(std::string uri) const
  {
    std::unordered_set<std::string> closed_list;
    std::queue<std::string> fringe;
    fringe.push(uri);

    while (!fringe.empty())
    {
      // Get the next element and remove it from the fringe.
      std::string current_uri = fringe.front();
      fringe.pop();

      if (closed_list.find(current_uri) == closed_list.end())
      {
        // Not seen this uri yet...
        closed_list.insert(current_uri);

        // Parse the uri into triples.
        rdf_web_parser p;
        std::list<rdf_triple> triples;
        bool good_rdf = p(current_uri, std::back_inserter(triples));

        if (good_rdf)
        {
          // Remove the triples that do not match the supplied predicate.
          auto new_end =
            std::remove_if(std::begin(triples), std::end(triples), [&pred_](rdf_triple const& t)
              {
                return !pred_(t);
              });

          // Apply the visitor function to the triples that made it past the filter.
          func_(current_uri, std::begin(triples), new_end);

          // Add those triples that are uris to the fringe.
          std::for_each(
            std::begin(triples), new_end,
            [&fringe, &pred_](rdf_triple t)
            {
              // If the triple matches the predicate, add it to the fringe.
              auto obj = t.object();
              if (is_uri(obj))
              {
                auto next_uri = term_cast<rdf_uri>(obj);
                fringe.push(to_std_string(next_uri.uri()));
              }
            });
        }
      }
    }
  }
};

//===========================================================================
// Factory functions for easily creating ontology_walker objects.
//===========================================================================

namespace factories {

template <typename Function, typename Predicate>
ontology_walker<Function, Predicate>
make_ontology_walker(Function&& func, Predicate&& pred)
{
  return ontology_walker<Function, Predicate>(
    std::forward<Function>(func), std::forward<Predicate>(pred));
}

struct true_const_pred
{
  template <typename T>
  constexpr bool operator()(T const&) const { return true; }
};

template <typename Function>
ontology_walker<Function, true_const_pred>
make_ontology_walker(Function&& func)
{
  return ontology_walker<Function, true_const_pred>(
    std::forward<Function>(func), true_const_pred());
}

} // namespace factories
} // namespace rdf

#endif