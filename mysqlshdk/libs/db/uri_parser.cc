/*
 * Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms, as
 * designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mysqlshdk/libs/db/uri_parser.h"
#include <cctype>
#include "utils/utils_string.h"

// Avoid warnings from protobuf and rapidjson
#if defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wpedantic"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#ifndef __has_warning
#define __has_warning(x) 0
#endif
#if __has_warning("-Wunused-local-typedefs")
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4018 4996)
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif

namespace mysqlshdk {
namespace db {
namespace uri {
#define URI_SCHEME "scheme"
#define URI_TARGET "targetinfo"
#define URI_USER_INFO "userinfo"
#define URI_PATH "path"
#define URI_QUERY "query"

#define IS_DELIMITER(x) DELIMITERS.find(x) != std::string::npos
#define IS_SUBDELIMITER(x) SUBDELIMITERS.find(x) != std::string::npos
#define IS_ALPHA(x) ALPHA.find(x) != std::string::npos
#define IS_DIGIT(x) DIGIT.find(x) != std::string::npos
#define IS_HEXDIG(x) HEXDIG.find(x) != std::string::npos
#define IS_ALPHANUMERIC(x) ALPHANUMERIC.find(x) != std::string::npos
#define IS_RESERVED(x) RESERVED.find(x) != std::string::npos
#define IS_UNRESERVED(x) UNRESERVED.find(x) != std::string::npos

std::map<char, char> Uri_parser::hex_literals = {
    {'1', 1},  {'2', 2},  {'3', 3},  {'4', 4},  {'5', 5},  {'6', 6},
    {'7', 7},  {'8', 8},  {'9', 9},  {'A', 10}, {'B', 11}, {'C', 12},
    {'D', 13}, {'E', 14}, {'F', 15}, {'a', 10}, {'b', 11}, {'c', 12},
    {'d', 13}, {'e', 14}, {'f', 15}};

Uri_parser::Uri_parser() {}

void Uri_parser::parse_scheme() {
  if (_chunks.find(URI_SCHEME) != _chunks.end()) {
    // Does schema parsing based on RFC3986
    _tokenizer.reset();
    _tokenizer.set_allow_spaces(false);

    // RFC3986 Defines the next valid chars: +.-
    // However, in the Specification we only consider alphanumerics and + for
    // extensions
    _tokenizer.set_simple_tokens("+");
    _tokenizer.set_complex_token("alphanumeric", ALPHANUMERIC);

    _tokenizer.process(_chunks[URI_SCHEME]);

    std::string scheme = _tokenizer.consume_token("alphanumeric");

    if (_tokenizer.tokens_available()) {
      std::string scheme_ext;
      _tokenizer.consume_token("+");
      scheme_ext = _tokenizer.consume_token("alphanumeric");

      if (_tokenizer.tokens_available())
        throw std::invalid_argument(shcore::str_format(
            "Invalid scheme format [%s], only one extension is supported",
            get_input_chunk(_chunks[URI_SCHEME]).c_str()));
      else
        throw std::invalid_argument(shcore::str_format(
            "Scheme extension [%s] is not supported", scheme_ext.c_str()));

      // TODO(rennox): Internally not supporting URI shcheme extensions
      // _data->set_scheme_ext(scheme_ext);
    }

    // Validate on unique supported schema formats
    // In the future we may support additional stuff, like extensions
    if (scheme != "mysql" && scheme != "mysqlx")
      throw std::invalid_argument(
          shcore::str_format("Invalid scheme [%s], supported schemes "
                             "include: mysql, mysqlx",
                             scheme.c_str()));

    _data->set_scheme(scheme);
  }
}

void Uri_parser::parse_userinfo() {
  if (_chunks.find(URI_USER_INFO) != _chunks.end()) {
    _tokenizer.reset();

    _tokenizer.set_allow_spaces(false);
    _tokenizer.set_complex_token("pct-encoded", {"%", HEXDIG, HEXDIG});
    _tokenizer.set_complex_token("unreserved", UNRESERVED);
    _tokenizer.set_complex_token("sub-delims", SUBDELIMITERS);
    _tokenizer.set_simple_tokens(":");
    _tokenizer.set_final_token_group("password", ":");

    _tokenizer.process(_chunks[URI_USER_INFO]);

    std::string user, password;
    bool has_password = false;
    while (_tokenizer.tokens_available()) {
      if (_tokenizer.cur_token_type_is(":"))
        has_password = true;
      else if (_tokenizer.cur_token_type_is("pct-encoded"))
        (has_password ? password : user) +=
            percent_decode(_tokenizer.peek_token().get_text());
      else
        (has_password ? password : user) += _tokenizer.peek_token().get_text();

      _tokenizer.consume_any_token();
    }

    // At this point the user name can't be empty
    if (user.empty()) throw std::invalid_argument("Missing user name");

    _data->set_user(user);
    if (has_password) _data->set_password(password);
  }
}

void Uri_parser::parse_target() {
  if (_chunks.find(URI_TARGET) != _chunks.end()) {
    size_t start = _chunks[URI_TARGET].first;
    size_t end = _chunks[URI_TARGET].second;
    if (input_contains(".", start) || input_contains("/", start) ||
        input_contains("(.", start) || input_contains("(/", start)) {
      auto offset = _chunks[URI_TARGET].first;
      {
        // When it starts with / the symbol is skipped and rest is parsed
        // as unencoded value
        std::string socket;
        if (_input[start] == '/') {
          socket = "/";
          start++;
          offset++;
        }

        socket += parse_value({start, end}, &offset, "");

        _data->set_socket(socket);
      }

      if (offset <= _chunks[URI_TARGET].second)
        throw std::invalid_argument(
            "Unexpected data [" +
            get_input_chunk({offset, _chunks[URI_TARGET].second}) +
            "] at position " + std::to_string(offset));

      // Windows pipe
    } else if (input_contains("\\.", start)) {
      start += 2;
      size_t offset(start);
      _data->set_pipe(parse_value({start, end}, &offset, ""));

      if (offset <= _chunks[URI_TARGET].second)
        throw std::invalid_argument(
            "Unexpected data [" +
            get_input_chunk({offset, _chunks[URI_TARGET].second}) +
            "] at position " + std::to_string(offset));
    } else {
      parse_host();
    }
  }
}

std::string Uri_parser::parse_ipv4(size_t *offset) {
  std::string ret_val;
  std::string host;

  if (_tokenizer.cur_token_type_is("digits") &&
      _tokenizer.next_token_type(".") &&
      _tokenizer.next_token_type("digits", 2) &&
      _tokenizer.next_token_type(".", 3) &&
      _tokenizer.next_token_type("digits", 4) &&
      _tokenizer.next_token_type(".", 5) &&
      _tokenizer.next_token_type("digits", 6)) {
    for (size_t index = 0; index < 4; index++) {
      std::string octet = _tokenizer.consume_token("digits");
      int value;
      try {
        value = std::stoi(octet);
      } catch (const std::invalid_argument &e) {
        std::string s = "Error parsing IPV4 address: ";
        s += e.what();
        throw std::invalid_argument(s);
      }
      if (value < 0 || value > 255) {
        throw std::invalid_argument(
            "Octet value out of bounds [" + octet +
            "], valid range for IPv4 is 0 to 255 at position " +
            std::to_string(*offset));
      } else {
        host += octet;
        (*offset) += octet.length();

        if (index < 3) {
          host += _tokenizer.consume_token(".");
          (*offset)++;
        }
      }
    }

    ret_val = host;
  }

  return host;
}

void Uri_parser::parse_ipv6(const std::pair<size_t, size_t> &range,
                            size_t *offset) {
  _tokenizer.reset();
  _tokenizer.set_allow_spaces(false);
  _tokenizer.set_simple_tokens(":.[]");
  _tokenizer.set_complex_token("digits", DIGIT);
  _tokenizer.set_complex_token("hex-digits", HEXDIG);

  _tokenizer.process(range);

  _tokenizer.consume_token("[");
  (*offset)++;

  std::vector<std::string> values;
  std::string next_type;
  std::string host;
  bool ipv4_found = false;
  bool ipv4_allowed = false;
  bool colon_allowed = false;
  bool double_colon_allowed = true;
  bool last_was_colon = false;
  int segment_count = 0;
  while (!_tokenizer.cur_token_type_is("]")) {
    if (host.empty()) {
      // An IP Address may begin with ::
      if (_tokenizer.cur_token_type_is(":")) {
        host += _tokenizer.consume_token(":");
        host += _tokenizer.consume_token(":");
        (*offset) += 2;
        colon_allowed = false;
        double_colon_allowed = false;
        ipv4_allowed = true;
        last_was_colon = true;
      } else {
        std::string value;
        auto token = _tokenizer.peek_token();
        while (token.get_type() == "hex-digits" ||
               token.get_type() == "digits") {
          value += _tokenizer.consume_any_token().get_text();
          token = _tokenizer.peek_token();
        }

        if (value.length() > 4)
          throw std::invalid_argument(
              "Invalid IPv6 value [" + value +
              "], maximum 4 hexadecimal digits accepted");

        host += value;
        (*offset) += value.length();

        segment_count++;

        colon_allowed = true;
        double_colon_allowed = true;
      }
    } else {
      // If an IPv4 is read, it must be at the end of the IPv6 definition
      // So we are done
      std::string ipv4_host;
      if (ipv4_allowed) ipv4_host = parse_ipv4(offset);

      if (ipv4_allowed && !ipv4_host.empty()) {
        host += ipv4_host;
        ipv4_found = true;
        break;

        // Colon is allowed after each hex-digit or after one colon
      } else if (colon_allowed && _tokenizer.cur_token_type_is(":")) {
        host += _tokenizer.consume_token(":");
        (*offset)++;

        if (last_was_colon) double_colon_allowed = false;

        colon_allowed = double_colon_allowed;

        // IPv4 is allowed after any colon
        ipv4_allowed = true;

        last_was_colon = true;
      } else {
        if (last_was_colon) {
          std::string value;
          auto token = _tokenizer.peek_token();
          while (token.get_type() == "hex-digits" ||
                 token.get_type() == "digits") {
            value += _tokenizer.consume_any_token().get_text();
            token = _tokenizer.peek_token();
          }
          if (value.empty())
            throw std::invalid_argument(
                shcore::str_format("Unexpected data [%s] found at position %u",
                                   _tokenizer.peek_token().get_text().c_str(),
                                   static_cast<uint32_t>(*offset)));
          else if (value.length() > 4)
            throw std::invalid_argument(
                "Invalid IPv6 value [" + value +
                "], maximum 4 hexadecimal digits accepted");

          host += value;
          (*offset) += value.length();

          segment_count++;
        } else {
          host += _tokenizer.consume_token(":");
          (*offset)++;
        }

        last_was_colon = !last_was_colon;

        // Colon is allowed only if the previous token was hex-digits
        // Or if the double colon is still an option
        colon_allowed = (!last_was_colon || double_colon_allowed);

        ipv4_allowed = last_was_colon;
      }
    }
  }

  // At this point we should be done wiht the IPv6 Address
  _tokenizer.consume_token("]");
  (*offset)++;

  // The shortcut :: was not used
  if (double_colon_allowed) {
    if ((ipv4_found && segment_count != 6) ||
        (!ipv4_found && segment_count != 8))
      throw std::invalid_argument(
          "Invalid IPv6: the number of segments does not match the "
          "specification");
  } else {
    if ((ipv4_found && segment_count >= 6) ||
        (!ipv4_found && segment_count >= 8))
      throw std::invalid_argument(
          "Invalid IPv6: the number of segments does not match the "
          "specification");
  }

  _data->set_host(host);
}

void Uri_parser::parse_port(const std::pair<size_t, size_t> &range,
                            size_t *offset) {
  _tokenizer.reset();
  _tokenizer.set_allow_spaces(false);
  _tokenizer.set_simple_tokens(":");
  _tokenizer.set_complex_token("digits", DIGIT);
  _tokenizer.process(range);

  _tokenizer.consume_token(":");
  (*offset)++;

  if (_tokenizer.tokens_available()) {
    std::string str_port = _tokenizer.consume_token("digits");
    (*offset) += str_port.length();
    int port = std::stoi(str_port);

    if (port < 0 || port > 65535)
      throw std::invalid_argument("Port is out of the valid range: 0 - 65535");

    _data->set_port(port);
  } else {
    throw std::invalid_argument("Missing port number");
  }

  if (_tokenizer.tokens_available())
    throw std::invalid_argument(
        shcore::str_format("Unexpected data [%s] found at position %u",
                           get_input_chunk({*offset, range.second}).c_str(),
                           static_cast<uint32_t>(*offset)));
}

void Uri_parser::parse_host() {
  size_t offset = _chunks[URI_TARGET].first;

  if (_input[_chunks[URI_TARGET].first] == '[') {
    parse_ipv6(_chunks[URI_TARGET], &offset);
  } else {
    _tokenizer.reset();
    _tokenizer.set_allow_spaces(false);
    _tokenizer.set_simple_tokens(".:");
    _tokenizer.set_complex_token("pct-encoded", {"%", HEXDIG, HEXDIG});
    _tokenizer.set_complex_token("digits", DIGIT);
    _tokenizer.set_complex_token("hex-digits", HEXDIG);
    _tokenizer.set_complex_token("sub-delims", SUBDELIMITERS);
    _tokenizer.set_complex_token("unreserved", UNRESERVED);

    _tokenizer.process(_chunks[URI_TARGET]);

    std::string host = parse_ipv4(&offset);
    if (host.empty()) {
      while (_tokenizer.tokens_available() &&
             !_tokenizer.cur_token_type_is(":")) {
        std::string data = _tokenizer.peek_token().get_text();
        if (_tokenizer.cur_token_type_is("pct-encoded"))
          host += percent_decode(data);
        else
          host += data;

        offset += data.length();

        _tokenizer.consume_any_token();
      }
    }

    _data->set_host(host);
  }

  if (offset <= _chunks[URI_TARGET].second)
    parse_port({offset, _chunks[URI_TARGET].second}, &offset);
}

void Uri_parser::parse_path() {
  if (_chunks.find(URI_PATH) != _chunks.end()) {
    _tokenizer.reset();
    _tokenizer.set_allow_spaces(false);
    _tokenizer.set_simple_tokens(":@");
    _tokenizer.set_complex_token("pct-encoded", {"%", HEXDIG, HEXDIG});
    _tokenizer.set_complex_token("unreserved", UNRESERVED);
    _tokenizer.set_complex_token("sub-delims", SUBDELIMITERS);

    _tokenizer.process(_chunks[URI_PATH]);

    std::string schema;
    while (_tokenizer.tokens_available()) {
      if (_tokenizer.cur_token_type_is("pct-encoded"))
        schema += percent_decode(_tokenizer.peek_token().get_text());
      else
        schema += _tokenizer.peek_token().get_text();

      _tokenizer.consume_any_token();
    }

    if (!schema.empty()) _data->set_schema(schema);
  }
}

void Uri_parser::parse_query() {
  size_t offset = _chunks[URI_QUERY].first;
  while (offset < _chunks[URI_QUERY].second)
    parse_attribute(_chunks[URI_QUERY], &offset);
}

void Uri_parser::parse_attribute(const std::pair<size_t, size_t> &range,
                                 size_t *offset) {
  _tokenizer.reset();
  _tokenizer.set_allow_spaces(false);
  _tokenizer.set_complex_token("pct-encoded", {"%", HEXDIG, HEXDIG});
  _tokenizer.set_complex_token("unreserved", UNRESERVED);
  // NOTE: The URI grammar specifies that sub-delims should be included on the
  // key production rule
  //       However sub-delims include the = and & chars which are used to join
  //       several key/value pairs and to associate a key's value, so we have
  //       excluded them.
  // tok.set_complex_token("sub-delims", SUBDELIMITERS);
  _tokenizer.set_complex_token("sub-delims", std::string("!$'()*+,;"));
  _tokenizer.set_final_token_group("pause", "=&");

  // We will skip the first char which is always a delimiter
  (*offset)++;

  // Pauses the processing on the presense of the next tokens
  _tokenizer.process({*offset, range.second});

  std::string attribute;
  bool has_value = false;
  while (_tokenizer.tokens_available()) {
    if (_tokenizer.cur_token_type_is("pause")) {
      if (_tokenizer.peek_token().get_text()[0] == '=') has_value = true;

      break;
    } else if (_tokenizer.cur_token_type_is("pct-encoded")) {
      attribute += percent_decode(_tokenizer.peek_token().get_text());
    } else {
      attribute += _tokenizer.peek_token().get_text();
    }

    _tokenizer.consume_any_token();
  }

  // Skips to the position right after the attribute or the = symbol
  size_t to_skip = attribute.length() + (has_value ? 1 : 0);
  (*offset) += to_skip;

  if (has_value)
    _data->set(attribute, parse_values({*offset, range.second}, offset));
  else
    _data->set(attribute, {});
}

std::vector<std::string> Uri_parser::parse_values(
    const std::pair<size_t, size_t> &range, size_t *offset) {
  std::vector<std::string> ret_val;

  auto closing = _input.find(']');
  if (_input[range.first] == '[' && closing != std::string::npos &&
      closing <= range.second) {
    while (_input[*offset] != ']') {
      // Next is a delimiter
      (*offset)++;
      std::string value = parse_value({*offset, closing - 1}, offset, ",]");
      ret_val.push_back(value);
    }

    // Skips the closing ]
    (*offset)++;
  } else {
    auto value = parse_value(range, offset, "&");
    ret_val.push_back(value);
  }

  return ret_val;
}

std::string Uri_parser::parse_value(const std::pair<size_t, size_t> &range,
                                    size_t *offset,
                                    const std::string &finalizers) {
  std::string ret_val;

  auto closing = _input.find(')', range.first);
  if (_input[range.first] == '(' && closing != std::string::npos &&
      closing <= range.second) {
    (*offset)++;
    ret_val = parse_unencoded_value({range.first + 1, closing - 1}, offset);
    (*offset)++;
  } else {
    ret_val = parse_encoded_value(range, offset, finalizers);
  }

  return ret_val;
}

char Uri_parser::percent_decode(const std::string &value) {
  int ret_val = 0;

  ret_val += hex_literals[value[1]] * 16;
  ret_val += hex_literals[value[2]];

  return ret_val;
}

std::string Uri_parser::get_input_chunk(
    const std::pair<size_t, size_t> &range) {
  return _input.substr(range.first, range.second - range.first + 1);
}

std::string Uri_parser::parse_unencoded_value(
    const std::pair<size_t, size_t> &range, size_t *offset,
    const std::string &finalizers) {
  _tokenizer.reset();
  _tokenizer.set_complex_token("pct-encoded", {"%", HEXDIG, HEXDIG});
#ifdef _WIN32
  _tokenizer.set_complex_token("unreserved",
                               std::string(UNRESERVED).append("\\:"));
#else
  _tokenizer.set_complex_token("unreserved",
                               std::string(UNRESERVED).append("/"));
#endif
  _tokenizer.set_complex_token("delims", DELIMITERS);

  if (!finalizers.empty()) _tokenizer.set_final_token_group("end", finalizers);

  _tokenizer.process(range);

  std::string value;

  // Reserves enough space for the value
  auto last_token = _tokenizer.peek_last_token();
  if (last_token)
    value.reserve((last_token->get_pos() + last_token->get_text().size()) -
                  range.first);

  // TODO(rennox): Add encoding logic for each appended token
  //       Yes it is unencoded value, but we support encoded stuff as well
  while (_tokenizer.tokens_available()) {
    auto token = _tokenizer.consume_any_token();
    if (token.get_type() == "pct-encoded")
      value += percent_decode(token.get_text());
    else
      value += token.get_text();

    (*offset) += token.get_text().length();
  }

  return value;
}

std::string Uri_parser::parse_encoded_value(
    const std::pair<size_t, size_t> &range, size_t *offset,
    const std::string &finalizers) {
  _tokenizer.reset();
  _tokenizer.set_complex_token("pct-encoded", {"%", HEXDIG, HEXDIG});
  _tokenizer.set_complex_token("unreserved", UNRESERVED);
  _tokenizer.set_complex_token("delims", std::string("!$'()*+;="));

  if (!finalizers.empty()) _tokenizer.set_final_token_group("end", finalizers);

  _tokenizer.process(range);

  std::string value;

  // Reserves enough space for the value
  auto last_token = _tokenizer.peek_last_token();
  if (last_token) value.reserve(last_token->get_pos() - range.first);

  // TODO(rennox): Add encoding logic for each appended token
  //       Yes it is unencoded value, but we support encoded stuff as well
  while (_tokenizer.tokens_available() &&
         !_tokenizer.cur_token_type_is("end")) {
    auto token = _tokenizer.consume_any_token();
    if (token.get_type() == "pct-encoded")
      value += percent_decode(token.get_text());
    else
      value += token.get_text();

    (*offset) += token.get_text().length();
  }

  return value;
}

Connection_options Uri_parser::parse(const std::string &input,
                                     Comparison_mode mode) {
  Connection_options data(mode);

  _data = &data;

  _input = input;
  _tokenizer.set_input(input);

  // Starts by splitting the major components
  // 1) Trims from the input the scheme component if found
  // first_char points to the first unassigned char
  // which at the end will be the same as the start of the target
  // (host/socket/pipe)
  size_t first_char = 0;
  size_t last_char = input.size() - 1;

  size_t position = input.find("://");
  if (position != std::string::npos) {
    if (position) {
      _chunks[URI_SCHEME] = {0, position - 1};
      first_char = position + 3;
    } else {
      throw std::invalid_argument("Scheme is missing");
    }
  }

  // 2) Trims from the input the userinfo component if found
  // It is safe to look for the first @ symbol since before the
  // userinfo@targetinfo the @ should come pct-encoded. i.e. in a password
  position = input.find("@", first_char);
  if (position != std::string::npos) {
    if (position > first_char) {
      _chunks[URI_USER_INFO] = {first_char, position - 1};
      first_char = position + 1;
    } else {
      throw std::invalid_argument("Missing user information");
    }
  }

  // 3) Trimming the query component, it starts with a question mark
  // last_char will point to the last unassigned char
  // If there was a query component, it ahead of this position
  position = input.find("?", first_char);
  if (position != std::string::npos) {
    _chunks[URI_QUERY] = {position, last_char};
    last_char = position - 1;
  }

  // 4) Gotta find the path component if any, path component starts with /
  //    but if the target is a socket it may also start with / so we need to
  //    find the right / defining a path component
  // Looks for the last / on the unassigned range
  // first_char points to the beggining of the target definition
  // so if / is on the first position it is not the path but the socket
  // definition
  position = input.rfind("/", last_char);
  bool has_path = false;
  if (position != std::string::npos && position > first_char) {
    // If the / is found at the second position, it could also be a socket
    // definintion in the form of: (/path/to/socket)
    // So we nede to ensure the found / is after the closing ) in this case
    if (input_contains("(/", first_char) ||
        input_contains("\\.(", first_char)) {
      size_t closing = input.find(")", first_char);
      has_path = closing != std::string::npos && position > closing;
    } else {
      // The found / was not a socket definition at all
      has_path = true;
    }

    // Path component was found
    if (has_path) {
      if (position < last_char) {
        _chunks[URI_PATH] = {position + 1, last_char};
        last_char = position - 1;
      } else {
        last_char--;
      }
    }
  }

  // 5) The rest is target info
  if (first_char <= last_char)
    _chunks[URI_TARGET] = {first_char, last_char};
  else
    throw std::invalid_argument("Missing target server information");

  parse_scheme();
  parse_userinfo();
  parse_target();
  parse_path();
  parse_query();

  return data;
}

bool Uri_parser::input_contains(const std::string &what, size_t index) {
  bool ret_val = false;

  if (index == std::string::npos)
    ret_val = _input.find(what) != std::string::npos;
  else
    ret_val = _input.find(what, index) == index;

  return ret_val;
}

}  // namespace uri
}  // namespace db
}  // namespace mysqlshdk
