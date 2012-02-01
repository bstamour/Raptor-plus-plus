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
// This header file contains the declarations of the core data structures
// of this rdf parsing library. Since these types are simple and do not
// rely on templates, the definitions of the functions are all located in
// the file rdf_parser.cpp.
//
// The reason for this separation is that this code needs to be linked
// against raptor2 and curl anyways, so creating an object file is
// required.
//===========================================================================

#ifndef BST_RDF_PARSER_HPP_
#define BST_RDF_PARSER_HPP_

#include <raptor2/raptor2.h>
#include <curl/curl.h>

#include <boost/variant.hpp>

#include <list>
#include <set>
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <algorithm>

#include <cstdio>

namespace rdf {

//===========================================================================
// unsigned strings are just std::strings using unsigned chars instead of
// normal ones. We delay the conversion to regular chars until they need to
// be printed out.
//===========================================================================
typedef std::basic_string<unsigned char> unsigned_string;

std::string to_std_string(unsigned_string const& str)
{
  std::string temp;
  std::transform(std::begin(str), std::end(str), std::back_inserter(temp), [](unsigned char c) {
      return static_cast<char>(c);    // DANGER!! HIGH VOLTAGE!!!
    });
  return temp;
}

std::ostream& operator<<(std::ostream& os, unsigned_string const& str)
{
  os << to_std_string(str);
  return os;
}

//===========================================================================
// These three structs represent the types that an rdf_term can be:
// 1) A uri
// 2) A literal value
// 3) A blank value
//===========================================================================

//----------------------------------------------------------------------
// A type representing a URI.
//----------------------------------------------------------------------
struct rdf_uri
{
  explicit rdf_uri(raptor_uri* uri)
    : uri_(uri == NULL ? unsigned_string() : raptor_uri_as_string(uri))
  {}

  unsigned_string uri() const { return uri_; }

private:
  unsigned_string uri_;
};

//----------------------------------------------------------------------
// A type representing a literal value.
//----------------------------------------------------------------------
struct rdf_literal
{
  explicit rdf_literal(raptor_term_literal_value lit)
    : literal_(lit.string, lit.string_len),
      literal_uri_(lit.datatype)
  {}

  unsigned_string value() const { return literal_; }
  rdf_uri uri() const { return literal_uri_; }

private:
  unsigned_string literal_;
  rdf_uri literal_uri_;
};

//----------------------------------------------------------------------
// A blank type.
//----------------------------------------------------------------------
struct rdf_blank
{
  explicit rdf_blank(raptor_term_blank_value blnk)
    : str_(blnk.string, blnk.string_len)
  {}

  unsigned_string value() const { return str_; }

private:
  unsigned_string str_;
};

//===========================================================================
// Using boost.variant to represent an rdf_term, since a term can be one
// of three different things. boost.variant can be thought of in this case
// as a type-safe union.
//===========================================================================
typedef boost::variant<rdf_uri, rdf_literal, rdf_blank> rdf_term;

//----------------------------------------------------------------------
// Predicates to detect the type of the rdf_term.
//----------------------------------------------------------------------

namespace {

struct is_uri_pred_visitor : boost::static_visitor<bool>
{
  bool operator()(rdf_uri const&) const { return true; }
  template <typename T> bool operator()(T const&) const { return false; }
};

struct is_literal_pred_visitor : boost::static_visitor<bool>
{
  bool operator()(rdf_literal const&) const { return true; }
  template <typename T> bool operator()(T const&) const { return false; }
};

struct is_blank_pred_visitor : boost::static_visitor<bool>
{
  bool operator()(rdf_blank const&) const { return true; }
  template <typename T> bool operator()(T const&) const { return false; }
};

} // namespace

inline bool is_uri(rdf_term& t)
{
  return boost::apply_visitor(is_uri_pred_visitor(), t);
}

inline bool is_literal(rdf_term& t)
{
  return boost::apply_visitor(is_literal_pred_visitor(), t);
}

inline bool is_blank(rdf_term& t)
{
  return boost::apply_visitor(is_blank_pred_visitor(), t);
}

//----------------------------------------------------------------------
// term_cast casts rdf_term types into their underlying types.
//----------------------------------------------------------------------

template <typename T> inline T term_cast(rdf_term&);

template <> inline rdf_uri term_cast<rdf_uri>(rdf_term& t)
{
  if (is_uri(t))
    return boost::get<rdf_uri>(t);
  else
    throw std::domain_error("bad cast");
}

template <> inline rdf_literal term_cast<rdf_literal>(rdf_term& t)
{
  if (is_literal(t))
    return boost::get<rdf_literal>(t);
  else
    throw std::domain_error("bad cast");
}

template <> inline rdf_blank term_cast<rdf_blank>(rdf_term& t)
{
  if (is_blank(t))
    return boost::get<rdf_blank>(t);
  else
    throw std::domain_error("bad cast");
}

rdf_term make_rdf_term(raptor_term* const rterm)
{
  switch (rterm->type)
  {
  case RAPTOR_TERM_TYPE_URI:
    return rdf_uri(rterm->value.uri);
  case RAPTOR_TERM_TYPE_LITERAL:
    return rdf_literal(rterm->value.literal);
  case RAPTOR_TERM_TYPE_BLANK:
    return rdf_blank(rterm->value.blank);
  default:
    throw std::domain_error("bad rdf data");
  }
}

namespace {

struct rdf_term_print_visitor : public boost::static_visitor<void>
{
  explicit rdf_term_print_visitor(std::ostream& os)
    : os_(os)
  {}

  void operator()(rdf_uri const& uri) const
  {
    os_ << uri.uri();
  }

  void operator()(rdf_literal const& lit) const
  {
    os_ << lit.value();
  }

  void operator()(rdf_blank const& blnk) const
  {
    os_ << blnk.value();
  }

private:
  std::ostream& os_;
};

} // namespace

std::ostream& operator<<(std::ostream& os, rdf_term const& rhs)
{
  rdf_term_print_visitor v(os);
  boost::apply_visitor(v, rhs);
  return os;
}

//===========================================================================
// An rdf triple is a <subject, predicate, object> triple where subject,
// predicate, and object are of type rdf_term.
//===========================================================================
class rdf_triple
{
public:
  rdf_triple(rdf_term const& s, rdf_term const& p, rdf_term const& o)
    : subject_(s), predicate_(p), object_(o)
  {}

  rdf_term subject() const { return subject_; }
  rdf_term predicate() const { return predicate_; }
  rdf_term object() const { return object_; }

private:
  rdf_term subject_;
  rdf_term predicate_;
  rdf_term object_;
};

std::ostream& operator<<(std::ostream& os, rdf_triple const& t)
{
  os << t.subject() << ' ' << t.predicate() << ' ' << t.object();
  return os;
}

//===========================================================================
// This object parses an rdf document from a local file.
//===========================================================================

// TODO: Need to work on this.

class rdf_parser
{
public:
  rdf_parser()
    : world_(raptor_new_world(), raptor_free_world),
      rdf_parser_(
        raptor_new_parser(world_.get(), "rdfxml"),
        raptor_free_parser
      )
  {}

  template <typename Iter>
  bool operator()(std::string const& file_name, Iter dest) const
  {
    std::cout << "top" << std::endl;


    std::vector<rdf_triple> triples;
    raptor_parser_set_statement_handler(
      rdf_parser_.get(),
      static_cast<void*>(&triples),
      &rdf_parser::handle_statement
    );

    bool good_parse = true;
    raptor_world_set_log_handler(
      world_.get(),
      static_cast<void*>(&good_parse),
      &rdf_parser::handle_log_messages
    );


      std::FILE* stream = std::fopen(file_name.c_str(), "rb");
      int x = raptor_parser_parse_file_stream(rdf_parser_.get(), stream, file_name.c_str(), NULL);
      std::fclose(stream);


      std::cout << x << std::endl;

      std::cout << "bottom" << std::endl;

      std::copy(std::begin(triples), std::end(triples), dest);

      return good_parse;
  }

protected:

  static void handle_statement(void* data, raptor_statement* statement)
  {
    std::cout << "statement handler" << std::endl;

    std::vector<rdf_triple>* triples = static_cast<std::vector<rdf_triple>*>(data);
    triples->push_back(rdf_triple(
        make_rdf_term(statement->subject),
        make_rdf_term(statement->predicate),
        make_rdf_term(statement->object)
      ));
  }

  static void handle_log_messages(void* data, raptor_log_message* message)
  {
    bool* good_parse = static_cast<bool*>(data);
    *good_parse = (
      message->level != RAPTOR_LOG_LEVEL_ERROR
      && message->level != RAPTOR_LOG_LEVEL_FATAL
    );

    //if (!(*good_parse))
      std::cerr << message->text << std::endl;
  }

private:
  std::shared_ptr<raptor_world> world_;
  std::shared_ptr<raptor_parser> rdf_parser_;
};

//===========================================================================
// This object encapsulates an rdf web parser from Raptor. Calling the
// member function parse( ) will download the rdf file and parse it into
// a vector of rdf triples.
//===========================================================================
class rdf_web_parser
{
public:
  rdf_web_parser()
    : world_(raptor_new_world(), raptor_free_world),
      rdf_parser_(
        raptor_new_parser(world_.get(), "rdfxml"),
        raptor_free_parser
      ),
      curl_conn_(curl_easy_init(), curl_easy_cleanup)
  {}

  template <typename Iter>
  bool operator()(std::string const& uri, Iter dest) const
  {
    //----------------------------------------------------
    // Set up the event handlers.
    //----------------------------------------------------

    std::vector<rdf_triple> triples;
    raptor_parser_set_statement_handler(
      rdf_parser_.get(),
      static_cast<void*>(&triples),
      &rdf_web_parser::handle_statement
    );

    bool good_parse = true;
    raptor_world_set_log_handler(
      world_.get(),
      static_cast<void*>(&good_parse),
      &rdf_web_parser::handle_log_messages
    );

    //----------------------------------------------------
    // Create the uri object and perform the parse.
    //----------------------------------------------------

    std::shared_ptr<raptor_uri> r_uri(
      raptor_new_uri(world_.get(), (unsigned char*)uri.c_str()),
      raptor_free_uri
    );

    if (r_uri.get() == NULL)
      throw std::domain_error("Failed to initialize raptor uri");

    // TODO: Check out the kinds of error codes this can create and
    // throw exceptions accordingly.
    /*int result = */raptor_parser_parse_uri_with_connection(
      rdf_parser_.get(), r_uri.get(), NULL, curl_conn_.get()
    );

    std::copy(std::begin(triples), std::end(triples), dest);

    return good_parse;
  }

protected:
  static void handle_statement(void* data, raptor_statement* statement)
  {
    std::vector<rdf_triple>* triples = static_cast<std::vector<rdf_triple>*>(data);
    triples->push_back(rdf_triple(
        make_rdf_term(statement->subject),
        make_rdf_term(statement->predicate),
        make_rdf_term(statement->object)
      ));
  }

  static void handle_log_messages(void* data, raptor_log_message* message)
  {
    bool* good_parse = static_cast<bool*>(data);
    *good_parse = (
      message->level != RAPTOR_LOG_LEVEL_ERROR
      && message->level != RAPTOR_LOG_LEVEL_FATAL
    );

    if (!(*good_parse))
      std::cerr << message->text << std::endl;
  }

private:
  std::shared_ptr<raptor_world> world_;
  std::shared_ptr<raptor_parser> rdf_parser_;
  std::shared_ptr<CURL> curl_conn_;
};

} // namespace rdf

#endif