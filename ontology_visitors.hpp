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
// This file contains a bunch of pre-built visitors for use with the
// rdf::ontology_walker object.
//===========================================================================

#ifndef BST_ONTOLOGY_VISITORS_HPP_
#define BST_ONTOLOGY_VISITORS_HPP_

#include "rdf_parser.hpp"

#include <list>
#include <utility>
#include <iostream>
#include <algorithm>

#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/algorithm/iteration.hpp>

namespace rdf { namespace visitors {

//===========================================================================
// Simply print the triples as they are discovered.
//===========================================================================
struct print_triples
{
  template <typename Iter>
  void operator()(std::string const&, Iter first, Iter last) const
  {
    std::for_each(first, last, [](rdf::rdf_triple const& t) {
        std::cout << t << "\n";
      });
    std::cout << "==============================================================" << std::endl;
  }
};

struct print_uris
{
  template <typename Iter>
  void operator()(std::string const& uri, Iter, Iter) const
  {
    std::cout << uri << "\n";
  }
};

struct output_triples
{
  explicit output_triples(std::ostream& os) : os_(os)
  {}

  template <typename Iter>
  void operator()(std::string const&, Iter first, Iter last) const
  {
    std::copy(first, last, std::ostream_iterator<typename Iter::value_type>(os_, "\n"));
  }

private:
  std::ostream& os_;
};

//===========================================================================
// Store the triples in a list.
//===========================================================================
struct store_triples
{
  explicit store_triples(std::list<rdf::rdf_triple>& lst)
    : triple_store_(lst)
  {}

  template <typename Iter>
  void operator()(std::string const&, Iter first, Iter last) const
  {
    std::copy(first, last, std::back_inserter(triple_store_));
  }

private:
  std::list<rdf::rdf_triple>& triple_store_;
};

//===========================================================================
// Store the triples if they match a given predicate.
//===========================================================================
template <typename Predicate>
struct store_triples_if
{
  explicit store_triples_if(std::list<rdf::rdf_triple>& lst, Predicate&& pred)
    : triple_store_(lst), pred_(std::forward<Predicate>(pred))
  {}

  template <typename Iter>
  void operator()(std::string const& uri, Iter first, Iter last) const
  {
    std::copy_if(first, last, std::back_inserter(triple_store_), pred_);
  }

private:
  std::list<rdf::rdf_triple>& triple_store_;
  Predicate&& pred_;
};

namespace factories {

template <typename Predicate>
rdf::visitors::store_triples_if<Predicate>
store_triples_if(std::list<rdf_triple>& lst, Predicate&& pred)
{
  return rdf::visitors::store_triples_if<Predicate>(lst, std::forward<Predicate>(pred));
}

} // namespace factories

//===========================================================================
// Store the uri's visited during the search.
//===========================================================================
struct store_uris
{
  explicit store_uris(std::list<std::string>& lst)
    : uris_(lst)
  {}

  template <typename Iter>
  void operator()(std::string const& str, Iter, Iter) const
  {
    uris_.push_back(str);
  }

private:
  std::list<std::string>& uris_;
};

//===========================================================================
// Count the number of nodes visited.
//===========================================================================
template <typename T>
struct count_nodes
{
  explicit count_nodes(T& sz) : size_(sz)
  {}

  template <typename Iter>
  void operator()(std::string const&, Iter, Iter) const
  { ++size_; }

private:
  T& size_;
};

//===========================================================================
// Combine multiple visitors together and call them all one by one when
// a node is visited.
//===========================================================================
template <typename... Functions>
class aggregate
{
  //----------------------------------------------------------------------
  // This unary functor is used internally by boost::fusion::for_each.
  //----------------------------------------------------------------------
  template <typename Iter>
  struct call_me
  {
    call_me(std::string const& str, Iter first, Iter last)
      : str_(str), first_(first), last_(last)
    {}

    template <typename Func>
    void operator()(Func&& f) const
    {
      f(str_, first_, last_);
    }

  private:
    std::string str_;
    Iter first_;
    Iter last_;
  };

  //----------------------------------------------------------------------
  // This code will not be needed with later versions of gcc.
  // Gcc has issues with template parameter unpacking, so we need to use
  // a wrapper struct to get the actual type...
  //----------------------------------------------------------------------
  template <template <typename...> class Container, typename... TS>
  struct container_wrapper
  {
    typedef Container<TS...> type;
  };

  //----------------------------------------------------------------------
  // Soon we should be able to just use:
  //
  // boost::fusion::vector<Functions...> as the type here...
  //----------------------------------------------------------------------
  typename container_wrapper<boost::fusion::vector, Functions...>::type functions_;

public:
  explicit aggregate(Functions&&... funcs)
    : functions_(std::forward<Functions>(funcs)...)
  {}

  template <typename Iter>
  void operator()(std::string const& str, Iter first, Iter last) const
  {
    call_me<Iter> caller(str, first, last);
    boost::fusion::for_each(functions_, caller);
  }
};

namespace factories {

template <typename... Funcs>
aggregate<Funcs...> make_aggregate(Funcs&&... funcs)
{
  return aggregate<Funcs...>(std::forward<Funcs>(funcs)...);
}

} // namespace factories

} // namespace visitors
} // namespace rdf

#endif