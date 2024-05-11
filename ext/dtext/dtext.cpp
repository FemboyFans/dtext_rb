
#line 1 "ext/dtext/dtext.cpp.rl"
#include "dtext.h"
#include "url.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <regex>

#ifdef DEBUG
#undef g_debug
#define STRINGIFY(x) XSTRINGIFY(x)
#define XSTRINGIFY(x) #x
#define g_debug(fmt, ...) fprintf(stderr, "\x1B[1;32mDEBUG\x1B[0m %-28.28s %-24.24s " fmt "\n", __FILE__ ":" STRINGIFY(__LINE__), __func__, ##__VA_ARGS__)
#else
#undef g_debug
#define g_debug(...)
#endif

static const size_t MAX_STACK_DEPTH = 512;

// Strip qualifier from tag: "Artoria Pendragon (Lancer) (Fate)" -> "Artoria Pendragon (Lancer)"
static const std::regex tag_qualifier_regex("[ _]\\([^)]+?\\)$");

// Permitted HTML attribute names.
static const std::unordered_map<std::string_view, const std::unordered_set<std::string_view>> permitted_attribute_names = {
  { "thead",    { "align" } },
  { "tbody",    { "align" } },
  { "tr",       { "align" } },
  { "td",       { "align", "colspan", "rowspan" } },
  { "th",       { "align", "colspan", "rowspan" } },
  { "col",      { "align", "span" } },
  { "colgroup", {} },
};

// Permitted HTML attribute values.
static const std::unordered_set<std::string_view> align_values = { "left", "center", "right", "justify" };
static const std::unordered_map<std::string_view, std::function<bool(std::string_view)>> permitted_attribute_values = {
  { "align",   [](auto value) { return align_values.find(value) != align_values.end(); } },
  { "span",    [](auto value) { return std::all_of(value.begin(), value.end(), isdigit); } },
  { "colspan", [](auto value) { return std::all_of(value.begin(), value.end(), isdigit); } },
  { "rowspan", [](auto value) { return std::all_of(value.begin(), value.end(), isdigit); } },
};

static unsigned char ascii_tolower(unsigned char c);


#line 788 "ext/dtext/dtext.cpp.rl"



#line 54 "ext/dtext/dtext.cpp"
static const int dtext_start = 1630;
static const int dtext_first_final = 1630;
static const int dtext_error = 0;

static const int dtext_en_basic_inline = 1656;
static const int dtext_en_inline = 1659;
static const int dtext_en_code = 1885;
static const int dtext_en_nodtext = 1889;
static const int dtext_en_table = 1893;
static const int dtext_en_main = 1630;


#line 791 "ext/dtext/dtext.cpp.rl"

void StateMachine::dstack_push(element_t element) {
  dstack.push_back(element);
}

element_t StateMachine::dstack_pop() {
  if (dstack.empty()) {
    g_debug("dstack pop empty stack");
    return DSTACK_EMPTY;
  } else {
    auto element = dstack.back();
    dstack.pop_back();
    return element;
  }
}

element_t StateMachine::dstack_peek() {
  return dstack.empty() ? DSTACK_EMPTY : dstack.back();
}

bool StateMachine::dstack_check(element_t expected_element) {
  return dstack_peek() == expected_element;
}

// Return true if the given tag is currently open.
bool StateMachine::dstack_is_open(element_t element) {
  return std::find(dstack.begin(), dstack.end(), element) != dstack.end();
}

int StateMachine::dstack_count(element_t element) {
  return std::count(dstack.begin(), dstack.end(), element);
}

bool StateMachine::is_inline_element(element_t type) {
  return type >= INLINE;
}

bool StateMachine::is_internal_url(const std::string_view url) {
  if (url.starts_with("/")) {
    return true;
  } else if (options.domain.empty() || url.empty()) {
    return false;
  } else {
    // Matches the domain name part of a URL.
    static const std::regex url_regex("^https?://(?:[^/?#]*@)?([^/?#:]+)", std::regex_constants::icase);

    std::match_results<std::string_view::const_iterator> matches;
    std::regex_search(url.begin(), url.end(), matches, url_regex);
    return matches[1] == options.domain;
  }
}

void StateMachine::append(const auto c) {
  output += c;
}

void StateMachine::append(const std::string_view string) {
  output += string;
}

void StateMachine::append_html_escaped(char s) {
  switch (s) {
    case '<': append("&lt;"); break;
    case '>': append("&gt;"); break;
    case '&': append("&amp;"); break;
    case '"': append("&quot;"); break;
    default:  append(s);
  }
}

void StateMachine::append_html_escaped(const std::string_view string) {
  for (const unsigned char c : string) {
    append_html_escaped(c);
  }
}

void StateMachine::append_uri_escaped(const std::string_view string) {
  static const char hex[] = "0123456789ABCDEF";

  for (const unsigned char c : string) {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.' || c == '~') {
      append(c);
    } else {
      append('%');
      append(hex[c >> 4]);
      append(hex[c & 0x0F]);
    }
  }
}

void StateMachine::append_relative_url(const auto url) {
  if ((url[0] == '/' || url[0] == '#') && !options.base_url.empty()) {
    append_html_escaped(options.base_url);
  }

  append_html_escaped(url);
}

void StateMachine::append_absolute_link(const std::string_view url, const std::string_view title, bool internal_url, bool escape_title) {
  if (internal_url) {
    append("<a class=\"dtext-link\" href=\"");
  } else if (url == title) {
    append("<a rel=\"external nofollow noreferrer\" class=\"dtext-link dtext-external-link\" href=\"");
  } else {
    append("<a rel=\"external nofollow noreferrer\" class=\"dtext-link dtext-external-link dtext-named-external-link\" href=\"");
  }

  append_html_escaped(url);
  append("\">");

  if (escape_title) {
    append_html_escaped(title);
  } else {
    append(title);
  }

  append("</a>");
}

void StateMachine::append_mention(const std::string_view name) {
  mentions.push_back(std::string{name});
  append("<a class=\"dtext-link dtext-user-mention-link\" data-user-name=\"");
  append_html_escaped(name);
  append("\" href=\"");
  append_relative_url("/users?name=");
  append_uri_escaped(name);
  append("\">@");
  append_html_escaped(name);
  append("</a>");
}

void StateMachine::append_id_link(const char * title, const char * id_name, const char * url, const std::string_view id) {
  if (url[0] == '/') {
    append("<a class=\"dtext-link dtext-id-link dtext-");
    append(id_name);
    append("-id-link\" href=\"");
    append_relative_url(url);
  } else {
    append("<a rel=\"external nofollow noreferrer\" class=\"dtext-link dtext-id-link dtext-");
    append(id_name);
    append("-id-link\" href=\"");
    append_html_escaped(url);
  }

  append_uri_escaped(id);
  append("\">");
  append(title);
  append(" #");
  append_html_escaped(id);
  append("</a>");
}

void StateMachine::append_bare_unnamed_url(const std::string_view url) {
  auto [trimmed_url, leftovers] = trim_url(url);
  append_unnamed_url(trimmed_url);
  append_html_escaped(leftovers);
}

void StateMachine::append_unnamed_url(const std::string_view url) {
  DText::URL parsed_url(url);

  if (options.internal_domains.find(std::string(parsed_url.domain)) != options.internal_domains.end()) {
    append_internal_url(parsed_url);
  } else {
    append_absolute_link(url, url, parsed_url.domain == options.domain);
  }
}

void StateMachine::append_internal_url(const DText::URL& url) {
  auto path_components = url.path_components();
  auto query = url.query;
  auto fragment = url.fragment;

  if (path_components.size() == 2) {
    auto controller = path_components.at(0);
    auto id = path_components.at(1);

    if (!id.empty() && std::all_of(id.begin(), id.end(), ::isdigit)) {
      if (controller == "posts" && fragment.empty()) {
        // https://danbooru.donmai.us/posts/6000000#comment_2288996
        return append_id_link("post", "post", "/posts/", id);
      } else if (controller == "pools" && query.empty()) {
        // https://danbooru.donmai.us/pools/903?page=2
        return append_id_link("pool", "pool", "/pools/", id);
      } else if (controller == "comments") {
        return append_id_link("comment", "comment", "/comments/", id);
      } else if (controller == "forum_posts") {
        return append_id_link("forum", "forum-post", "/forum_posts/", id);
      } else if (controller == "forum_topics" && query.empty() && fragment.empty()) {
        // https://danbooru.donmai.us/forum_topics/1234?page=2
        // https://danbooru.donmai.us/forum_topics/1234#forum_post_5678
        return append_id_link("topic", "forum-topic", "/forum_topics/", id);
      } else if (controller == "users") {
        return append_id_link("user", "user", "/users/", id);
      } else if (controller == "artists") {
        return append_id_link("artist", "artist", "/artists/", id);
      } else if (controller == "notes") {
        return append_id_link("note", "note", "/notes/", id);
      } else if (controller == "favorite_groups" && query.empty()) {
        // https://danbooru.donmai.us/favorite_groups/1234?page=2
        return append_id_link("favgroup", "favorite-group", "/favorite_groups/", id);
      } else if (controller == "wiki_pages" && fragment.empty()) {
        // http://danbooru.donmai.us/wiki_pages/10933#dtext-self-upload
        return append_id_link("wiki", "wiki-page", "/wiki_pages/", id);
      }
    } else if (controller == "wiki_pages" && fragment.empty()) {
      return append_wiki_link({}, id, {}, id, {});
    }
  } else if (path_components.size() >= 3) {
    // http://danbooru.donmai.us/post/show/1234/touhou
    auto controller = path_components.at(0);
    auto action = path_components.at(1);
    auto id = path_components.at(2);

    if (!id.empty() && std::all_of(id.begin(), id.end(), ::isdigit)) {
      if (controller == "post" && action == "show") {
        return append_id_link("post", "post", "/posts/", id);
      }
    }
  }

  append_absolute_link(url.url, url.url, url.domain == options.domain);
}

void StateMachine::append_named_url(const std::string_view url, const std::string_view title) {
  auto parsed_title = parse_basic_inline(title);

  // protocol-relative url; treat `//example.com` like `http://example.com`
  if (url.size() > 2 && url.starts_with("//")) {
    auto full_url = "http:" + std::string(url);
    append_absolute_link(full_url, parsed_title, is_internal_url(full_url), false);
  } else if (url[0] == '/' || url[0] == '#') {
    append("<a class=\"dtext-link\" href=\"");
    append_relative_url(url);
    append("\">");
    append(parsed_title);
    append("</a>");
  } else if (url == title) {
    append_unnamed_url(url);
  } else {
    append_absolute_link(url, parsed_title, is_internal_url(url), false);
  }
}

void StateMachine::append_bare_named_url(const std::string_view url, std::string_view title) {
  auto [trimmed_url, leftovers] = trim_url(url);
  append_named_url(trimmed_url, title);
  append_html_escaped(leftovers);
}

void StateMachine::append_post_search_link(const std::string_view prefix, const std::string_view search, const std::string_view title, const std::string_view suffix) {
  auto normalized_title = std::string(title);

  append("<a class=\"dtext-link dtext-post-search-link\" href=\"");
  append_relative_url("/posts?tags=");
  append_uri_escaped(search);
  append("\">");

  // 19{{60s}} -> {{60s|1960s}}
  if (!prefix.empty()) {
    normalized_title.insert(0, prefix);
  }

  // {{pokemon_(creature)|}} -> {{pokemon_(creature)|pokemon}}
  if (title.empty()) {
    std::regex_replace(std::back_inserter(normalized_title), search.begin(), search.end(), tag_qualifier_regex, "");
  }

  // {{cat}}s -> {{cat|cats}}
  if (!suffix.empty()) {
    normalized_title.append(suffix);
  }

  append_html_escaped(normalized_title);
  append("</a>");

  clear_matches();
}

void StateMachine::append_wiki_link(const std::string_view prefix, const std::string_view tag, const std::string_view anchor, const std::string_view title, const std::string_view suffix) {
  auto normalized_tag = std::string(tag);
  auto title_string = std::string(title);

  // "Kantai Collection" -> "kantai_collection"
  std::transform(normalized_tag.cbegin(), normalized_tag.cend(), normalized_tag.begin(), [](unsigned char c) { return c == ' ' ? '_' : ascii_tolower(c); });

  // [[2019]] -> [[~2019]]
  if (std::all_of(normalized_tag.cbegin(), normalized_tag.cend(), ::isdigit)) {
    normalized_tag.insert(0, "~");
  }

  // Pipe trick: [[Kaga (Kantai Collection)|]] -> [[kaga_(kantai_collection)|Kaga]]
  if (title_string.empty()) {
    std::regex_replace(std::back_inserter(title_string), tag.cbegin(), tag.cend(), tag_qualifier_regex, "");
  }

  // 19[[60s]] -> [[60s|1960s]]
  if (!prefix.empty()) {
    title_string.insert(0, prefix);
  }

  // [[cat]]s -> [[cat|cats]]
  if (!suffix.empty()) {
    title_string.append(suffix);
  }

  append("<a class=\"dtext-link dtext-wiki-link\" href=\"");
  append_relative_url("/wiki_pages/");
  append_uri_escaped(normalized_tag);

  if (!anchor.empty()) {
    std::string normalized_anchor(anchor);
    std::transform(normalized_anchor.begin(), normalized_anchor.end(), normalized_anchor.begin(), [](char c) { return isalnum(c) ? ascii_tolower(c) : '-'; });
    append_html_escaped("#dtext-");
    append_html_escaped(normalized_anchor);
  }

  append("\">");
  append_html_escaped(title_string);
  append("</a>");

  wiki_pages.insert(std::string(tag));

  clear_matches();
}

void StateMachine::append_paged_link(const char * title, const std::string_view id, const char * tag, const char * href, const char * param, const std::string_view page) {
  append(tag);
  append_relative_url(href);
  append(id);
  append(param);
  append(page);
  append("\">");
  append(title);
  append(id);
  append("/p");
  append(page);
  append("</a>");
}

void StateMachine::append_dmail_key_link(const std::string_view dmail_id, const std::string_view dmail_key) {
  append("<a class=\"dtext-link dtext-id-link dtext-dmail-id-link\" href=\"");
  append_relative_url("/dmails/");
  append(dmail_id);
  append("?key=");
  append_uri_escaped(dmail_key);
  append("\">");
  append("dmail #");
  append(dmail_id);
  append("</a>");
}

void StateMachine::append_code_fence(const std::string_view code, const std::string_view language) {
  if (language.empty()) {
    append_block("<pre>");
    append_html_escaped(code);
    append_block("</pre>");
  } else {
    append_block("<pre class=\"language-");
    append_html_escaped(language);
    append_block("\">");
    append_html_escaped(code);
    append_block("</pre>");
  }
}

void StateMachine::append_inline_code(const std::string_view language) {
  if (language.empty()) {
    dstack_open_element(INLINE_CODE, "<code>");
  } else {
    dstack_open_element(INLINE_CODE, "<code class=\"language-");
    append_html_escaped(language);
    append("\">");
  }
}

void StateMachine::append_block_code(const std::string_view language) {
  dstack_close_leaf_blocks();

  if (language.empty()) {
    dstack_open_element(BLOCK_CODE, "<pre>");
  } else {
    dstack_open_element(BLOCK_CODE, "<pre class=\"language-");
    append_html_escaped(language);
    append("\">");
  }
}

void StateMachine::append_header(char header, const std::string_view id) {
  static element_t blocks[] = {BLOCK_H1, BLOCK_H2, BLOCK_H3, BLOCK_H4, BLOCK_H5, BLOCK_H6};
  element_t block = blocks[header - '1'];

  dstack_close_leaf_blocks();

  if (id.empty()) {
    dstack_open_element(block, "<h");
    append_block(header);
    append_block(">");
  } else {
    auto normalized_id = std::string(id);
    std::transform(id.begin(), id.end(), normalized_id.begin(), [](char c) { return isalnum(c) ? ascii_tolower(c) : '-'; });

    dstack_open_element(block, "<h");
    append_block(header);
    append_block(" id=\"dtext-");
    append_block(normalized_id);
    append_block("\">");
  }

  header_mode = true;
}

void StateMachine::append_block(const auto s) {
  if (!options.f_inline) {
    append(s);
  }
}

void StateMachine::append_block_html_escaped(const std::string_view string) {
  if (!options.f_inline) {
    append_html_escaped(string);
  }
}

void StateMachine::dstack_open_element(element_t type, const char * html) {
  g_debug("opening %s", html);

  dstack_push(type);

  if (type >= INLINE) {
    append(html);
  } else {
    append_block(html);
  }
}

void StateMachine::dstack_open_element_attributes(element_t type, std::string_view tag_name) {
  dstack_push(type);
  append_block("<");
  append_block(tag_name);

  auto& permitted_names = permitted_attribute_names.at(tag_name);
  for (auto& [name, value] : tag_attributes) {
    if (permitted_names.find(name) != permitted_names.end()) {
      auto validate_value = permitted_attribute_values.at(name);

      if (validate_value(value)) {
        append_block(" ");
        append_block_html_escaped(name);
        append_block("=\"");
        append_block_html_escaped(value);
        append_block("\"");
      }
    }
  }

  append_block(">");
  tag_attributes.clear();
}

bool StateMachine::dstack_close_element(element_t type, const std::string_view tag_name) {
  if (dstack_check(type)) {
    dstack_rewind();
    return true;
  } else if (type >= INLINE && dstack_peek() >= INLINE) {
    g_debug("out-of-order close %s; closing %s instead", element_names[type], element_names[dstack_peek()]);
    dstack_rewind();
    return true;
  } else if (type >= INLINE) {
    g_debug("out-of-order closing %s", element_names[type]);
    append_html_escaped(tag_name);
    return false;
  } else {
    g_debug("out-of-order closing %s", element_names[type]);
    append_block_html_escaped(tag_name);
    return false;
  }
}

// Close the last open tag.
void StateMachine::dstack_rewind() {
  element_t element = dstack_pop();
  g_debug("dstack rewind %s", element_names[element]);

  switch(element) {
    case BLOCK_P: append_block("</p>"); break;
    case INLINE_SPOILER: append("</span>"); break;
    case BLOCK_SPOILER: append_block("</div>"); break;
    case BLOCK_QUOTE: append_block("</blockquote>"); break;
    case BLOCK_EXPAND: append_block("</div></details>"); break;
    case BLOCK_NODTEXT: append_block("</p>"); break;
    case BLOCK_CODE: append_block("</pre>"); break;
    case BLOCK_TD: append_block("</td>"); break;
    case BLOCK_TH: append_block("</th>"); break;
    case BLOCK_COL: break; // <col> doesn't have a closing tag.

    case INLINE_NODTEXT: break;
    case INLINE_B: append("</strong>"); break;
    case INLINE_I: append("</em>"); break;
    case INLINE_U: append("</u>"); break;
    case INLINE_S: append("</s>"); break;
    case INLINE_TN: append("</span>"); break;
    case INLINE_CODE: append("</code>"); break;
    case INLINE_COLOR: append("</span>"); break;

    case BLOCK_TN: append_block("</p>"); break;
    case BLOCK_TABLE: append_block("</table>"); break;
    case BLOCK_COLGROUP: append_block("</colgroup>"); break;
    case BLOCK_THEAD: append_block("</thead>"); break;
    case BLOCK_TBODY: append_block("</tbody>"); break;
    case BLOCK_TR: append_block("</tr>"); break;
    case BLOCK_UL: append_block("</ul>"); break;
    case BLOCK_LI: append_block("</li>"); break;
    case BLOCK_H6: append_block("</h6>"); header_mode = false; break;
    case BLOCK_H5: append_block("</h5>"); header_mode = false; break;
    case BLOCK_H4: append_block("</h4>"); header_mode = false; break;
    case BLOCK_H3: append_block("</h3>"); header_mode = false; break;
    case BLOCK_H2: append_block("</h2>"); header_mode = false; break;
    case BLOCK_H1: append_block("</h1>"); header_mode = false; break;

    // Should never happen.
    case INLINE: break;
    case DSTACK_EMPTY: break;
  }
}

// container blocks: [spoiler], [quote], [expand], [tn]
// leaf blocks: [nodtext], [code], [table], [td]?, [th]?, <h1>, <p>, <li>, <ul>
void StateMachine::dstack_close_leaf_blocks() {
  g_debug("dstack close leaf blocks");

  while (!dstack.empty() && !dstack_check(BLOCK_QUOTE) && !dstack_check(BLOCK_SPOILER) && !dstack_check(BLOCK_EXPAND) && !dstack_check(BLOCK_TN)) {
    dstack_rewind();
  }
}

// Close all open tags up to and including the given tag.
void StateMachine::dstack_close_until(element_t element) {
  while (!dstack.empty() && !dstack_check(element)) {
    dstack_rewind();
  }

  dstack_rewind();
}

// Close all remaining open tags.
void StateMachine::dstack_close_all() {
  while (!dstack.empty()) {
    dstack_rewind();
  }
}

void StateMachine::dstack_open_list(int depth) {
  g_debug("open list");

  if (dstack_is_open(BLOCK_LI)) {
    dstack_close_until(BLOCK_LI);
  } else {
    dstack_close_leaf_blocks();
  }

  while (dstack_count(BLOCK_UL) < depth) {
    dstack_open_element(BLOCK_UL, "<ul>");
  }

  while (dstack_count(BLOCK_UL) > depth) {
    dstack_close_until(BLOCK_UL);
  }

  dstack_open_element(BLOCK_LI, "<li>");
}

void StateMachine::dstack_close_list() {
  while (dstack_is_open(BLOCK_UL)) {
    dstack_close_until(BLOCK_UL);
  }
}

void StateMachine::clear_matches() {
  a1 = NULL;
  a2 = NULL;
  b1 = NULL;
  b2 = NULL;
  c1 = NULL;
  c2 = NULL;
  d1 = NULL;
  d2 = NULL;
  e1 = NULL;
  e2 = NULL;
  f1 = NULL;
  f2 = NULL;
  g1 = NULL;
  g2 = NULL;
}

// True if a mention is allowed to start after this character.
static bool is_mention_boundary(unsigned char c) {
  switch (c) {
    case '\0': return true;
    case '\r': return true;
    case '\n': return true;
    case ' ':  return true;
    case '/':  return true;
    case '"':  return true;
    case '\'': return true;
    case '(':  return true;
    case ')':  return true;
    case '[':  return true;
    case ']':  return true;
    case '{':  return true;
    case '}':  return true;
    default:   return false;
  }
}

// Trim trailing unbalanced ')' characters from the URL.
std::tuple<std::string_view, std::string_view> StateMachine::trim_url(const std::string_view url) {
  std::string_view trimmed = url;

  while (!trimmed.empty() && trimmed.back() == ')' && std::count(trimmed.begin(), trimmed.end(), ')') > std::count(trimmed.begin(), trimmed.end(), '(')) {
    trimmed.remove_suffix(1);
  }

  return { trimmed, { trimmed.end(), url.end() } };
}

static unsigned char ascii_tolower(unsigned char c) {
  return (c >= 'A' && c <= 'Z') ? c ^ 0x20 : c;
}

// Replace CRLF sequences with LF.
static void replace_newlines(const std::string_view input, std::string& output) {
  size_t pos, last = 0;

  while (std::string::npos != (pos = input.find("\r\n", last))) {
    output.append(input, last, pos - last);
    output.append("\n");
    last = pos + 2;
  }

  output.append(input, last, pos - last);
}

StateMachine::StateMachine(const auto string, int initial_state, const DTextOptions options) : options(options) {
  // Add null bytes to the beginning and end of the string as start and end of string markers.
  input.reserve(string.size());
  input.append(1, '\0');
  replace_newlines(string, input);
  input.append(1, '\0');

  output.reserve(string.size() * 1.5);
  stack.reserve(16);
  dstack.reserve(16);

  p = input.c_str();
  pb = input.c_str();
  pe = input.c_str() + input.size();
  eof = pe;
  cs = initial_state;
}

std::string StateMachine::parse_inline(const std::string_view dtext) {
  StateMachine sm(dtext, dtext_en_inline, options);
  return sm.parse();
}

std::string StateMachine::parse_basic_inline(const std::string_view dtext) {
  DTextOptions opt = options;
  opt.max_thumbs = 0;
  StateMachine sm(dtext, dtext_en_basic_inline, options);
  return sm.parse();
}

StateMachine::ParseResult StateMachine::parse_dtext(const std::string_view dtext, DTextOptions options) {
  StateMachine sm(dtext, dtext_en_main, options);
  return { sm.parse(), sm.wiki_pages, sm.posts, sm.mentions };
}

std::string StateMachine::parse() {
  g_debug("parse '%.*s'", (int)(input.size() - 2), input.c_str() + 1);

  
#line 750 "ext/dtext/dtext.cpp"
	{
	( top) = 0;
	( ts) = 0;
	( te) = 0;
	( act) = 0;
	}

#line 1473 "ext/dtext/dtext.cpp.rl"
  
#line 760 "ext/dtext/dtext.cpp"
	{
	short _widec;
	if ( ( p) == ( pe) )
		goto _test_eof;
	goto _resume;

_again:
	switch ( ( cs) ) {
		case 1630: goto st1630;
		case 1631: goto st1631;
		case 1: goto st1;
		case 1632: goto st1632;
		case 2: goto st2;
		case 1633: goto st1633;
		case 3: goto st3;
		case 4: goto st4;
		case 5: goto st5;
		case 6: goto st6;
		case 7: goto st7;
		case 8: goto st8;
		case 9: goto st9;
		case 10: goto st10;
		case 11: goto st11;
		case 12: goto st12;
		case 13: goto st13;
		case 14: goto st14;
		case 15: goto st15;
		case 16: goto st16;
		case 1634: goto st1634;
		case 17: goto st17;
		case 18: goto st18;
		case 19: goto st19;
		case 20: goto st20;
		case 21: goto st21;
		case 22: goto st22;
		case 1635: goto st1635;
		case 23: goto st23;
		case 24: goto st24;
		case 25: goto st25;
		case 26: goto st26;
		case 27: goto st27;
		case 28: goto st28;
		case 29: goto st29;
		case 30: goto st30;
		case 31: goto st31;
		case 32: goto st32;
		case 33: goto st33;
		case 34: goto st34;
		case 35: goto st35;
		case 36: goto st36;
		case 37: goto st37;
		case 38: goto st38;
		case 39: goto st39;
		case 40: goto st40;
		case 41: goto st41;
		case 42: goto st42;
		case 43: goto st43;
		case 44: goto st44;
		case 1636: goto st1636;
		case 45: goto st45;
		case 46: goto st46;
		case 47: goto st47;
		case 48: goto st48;
		case 49: goto st49;
		case 50: goto st50;
		case 51: goto st51;
		case 52: goto st52;
		case 53: goto st53;
		case 54: goto st54;
		case 55: goto st55;
		case 56: goto st56;
		case 57: goto st57;
		case 58: goto st58;
		case 59: goto st59;
		case 1637: goto st1637;
		case 60: goto st60;
		case 61: goto st61;
		case 62: goto st62;
		case 63: goto st63;
		case 64: goto st64;
		case 65: goto st65;
		case 66: goto st66;
		case 67: goto st67;
		case 68: goto st68;
		case 69: goto st69;
		case 70: goto st70;
		case 71: goto st71;
		case 72: goto st72;
		case 73: goto st73;
		case 74: goto st74;
		case 1638: goto st1638;
		case 1639: goto st1639;
		case 75: goto st75;
		case 1640: goto st1640;
		case 1641: goto st1641;
		case 76: goto st76;
		case 0: goto st0;
		case 1642: goto st1642;
		case 77: goto st77;
		case 78: goto st78;
		case 79: goto st79;
		case 1643: goto st1643;
		case 1644: goto st1644;
		case 80: goto st80;
		case 81: goto st81;
		case 82: goto st82;
		case 83: goto st83;
		case 84: goto st84;
		case 85: goto st85;
		case 86: goto st86;
		case 87: goto st87;
		case 88: goto st88;
		case 89: goto st89;
		case 1645: goto st1645;
		case 90: goto st90;
		case 91: goto st91;
		case 92: goto st92;
		case 93: goto st93;
		case 94: goto st94;
		case 95: goto st95;
		case 96: goto st96;
		case 97: goto st97;
		case 98: goto st98;
		case 99: goto st99;
		case 1646: goto st1646;
		case 100: goto st100;
		case 101: goto st101;
		case 102: goto st102;
		case 103: goto st103;
		case 104: goto st104;
		case 105: goto st105;
		case 106: goto st106;
		case 1647: goto st1647;
		case 107: goto st107;
		case 1648: goto st1648;
		case 108: goto st108;
		case 109: goto st109;
		case 110: goto st110;
		case 111: goto st111;
		case 112: goto st112;
		case 113: goto st113;
		case 114: goto st114;
		case 115: goto st115;
		case 116: goto st116;
		case 1649: goto st1649;
		case 117: goto st117;
		case 1650: goto st1650;
		case 118: goto st118;
		case 119: goto st119;
		case 120: goto st120;
		case 121: goto st121;
		case 122: goto st122;
		case 123: goto st123;
		case 124: goto st124;
		case 1651: goto st1651;
		case 125: goto st125;
		case 126: goto st126;
		case 127: goto st127;
		case 128: goto st128;
		case 129: goto st129;
		case 130: goto st130;
		case 131: goto st131;
		case 132: goto st132;
		case 1652: goto st1652;
		case 133: goto st133;
		case 134: goto st134;
		case 135: goto st135;
		case 1653: goto st1653;
		case 136: goto st136;
		case 137: goto st137;
		case 138: goto st138;
		case 139: goto st139;
		case 140: goto st140;
		case 141: goto st141;
		case 142: goto st142;
		case 143: goto st143;
		case 144: goto st144;
		case 145: goto st145;
		case 146: goto st146;
		case 147: goto st147;
		case 148: goto st148;
		case 149: goto st149;
		case 150: goto st150;
		case 151: goto st151;
		case 152: goto st152;
		case 153: goto st153;
		case 154: goto st154;
		case 155: goto st155;
		case 156: goto st156;
		case 157: goto st157;
		case 158: goto st158;
		case 159: goto st159;
		case 160: goto st160;
		case 161: goto st161;
		case 162: goto st162;
		case 163: goto st163;
		case 164: goto st164;
		case 165: goto st165;
		case 166: goto st166;
		case 167: goto st167;
		case 168: goto st168;
		case 169: goto st169;
		case 170: goto st170;
		case 171: goto st171;
		case 172: goto st172;
		case 173: goto st173;
		case 1654: goto st1654;
		case 1655: goto st1655;
		case 1656: goto st1656;
		case 1657: goto st1657;
		case 174: goto st174;
		case 175: goto st175;
		case 176: goto st176;
		case 177: goto st177;
		case 178: goto st178;
		case 179: goto st179;
		case 180: goto st180;
		case 181: goto st181;
		case 182: goto st182;
		case 183: goto st183;
		case 184: goto st184;
		case 185: goto st185;
		case 186: goto st186;
		case 187: goto st187;
		case 188: goto st188;
		case 189: goto st189;
		case 190: goto st190;
		case 191: goto st191;
		case 192: goto st192;
		case 1658: goto st1658;
		case 193: goto st193;
		case 194: goto st194;
		case 195: goto st195;
		case 196: goto st196;
		case 197: goto st197;
		case 198: goto st198;
		case 199: goto st199;
		case 200: goto st200;
		case 201: goto st201;
		case 1659: goto st1659;
		case 1660: goto st1660;
		case 1661: goto st1661;
		case 202: goto st202;
		case 203: goto st203;
		case 204: goto st204;
		case 1662: goto st1662;
		case 1663: goto st1663;
		case 1664: goto st1664;
		case 205: goto st205;
		case 1665: goto st1665;
		case 206: goto st206;
		case 1666: goto st1666;
		case 207: goto st207;
		case 208: goto st208;
		case 209: goto st209;
		case 210: goto st210;
		case 211: goto st211;
		case 212: goto st212;
		case 213: goto st213;
		case 214: goto st214;
		case 215: goto st215;
		case 216: goto st216;
		case 217: goto st217;
		case 218: goto st218;
		case 219: goto st219;
		case 1667: goto st1667;
		case 220: goto st220;
		case 221: goto st221;
		case 222: goto st222;
		case 223: goto st223;
		case 224: goto st224;
		case 225: goto st225;
		case 1668: goto st1668;
		case 226: goto st226;
		case 227: goto st227;
		case 228: goto st228;
		case 229: goto st229;
		case 230: goto st230;
		case 231: goto st231;
		case 232: goto st232;
		case 233: goto st233;
		case 234: goto st234;
		case 235: goto st235;
		case 236: goto st236;
		case 237: goto st237;
		case 238: goto st238;
		case 239: goto st239;
		case 240: goto st240;
		case 241: goto st241;
		case 242: goto st242;
		case 243: goto st243;
		case 244: goto st244;
		case 245: goto st245;
		case 246: goto st246;
		case 1669: goto st1669;
		case 247: goto st247;
		case 248: goto st248;
		case 249: goto st249;
		case 250: goto st250;
		case 251: goto st251;
		case 252: goto st252;
		case 253: goto st253;
		case 254: goto st254;
		case 255: goto st255;
		case 256: goto st256;
		case 257: goto st257;
		case 258: goto st258;
		case 259: goto st259;
		case 260: goto st260;
		case 261: goto st261;
		case 262: goto st262;
		case 263: goto st263;
		case 264: goto st264;
		case 265: goto st265;
		case 266: goto st266;
		case 267: goto st267;
		case 268: goto st268;
		case 269: goto st269;
		case 270: goto st270;
		case 271: goto st271;
		case 272: goto st272;
		case 273: goto st273;
		case 274: goto st274;
		case 275: goto st275;
		case 276: goto st276;
		case 277: goto st277;
		case 278: goto st278;
		case 279: goto st279;
		case 280: goto st280;
		case 281: goto st281;
		case 1670: goto st1670;
		case 282: goto st282;
		case 283: goto st283;
		case 284: goto st284;
		case 285: goto st285;
		case 286: goto st286;
		case 287: goto st287;
		case 288: goto st288;
		case 289: goto st289;
		case 290: goto st290;
		case 291: goto st291;
		case 1671: goto st1671;
		case 1672: goto st1672;
		case 292: goto st292;
		case 293: goto st293;
		case 294: goto st294;
		case 295: goto st295;
		case 296: goto st296;
		case 297: goto st297;
		case 298: goto st298;
		case 299: goto st299;
		case 300: goto st300;
		case 301: goto st301;
		case 302: goto st302;
		case 303: goto st303;
		case 304: goto st304;
		case 305: goto st305;
		case 306: goto st306;
		case 307: goto st307;
		case 308: goto st308;
		case 309: goto st309;
		case 310: goto st310;
		case 311: goto st311;
		case 312: goto st312;
		case 313: goto st313;
		case 314: goto st314;
		case 315: goto st315;
		case 316: goto st316;
		case 317: goto st317;
		case 318: goto st318;
		case 319: goto st319;
		case 320: goto st320;
		case 321: goto st321;
		case 322: goto st322;
		case 323: goto st323;
		case 324: goto st324;
		case 325: goto st325;
		case 326: goto st326;
		case 327: goto st327;
		case 328: goto st328;
		case 329: goto st329;
		case 330: goto st330;
		case 331: goto st331;
		case 332: goto st332;
		case 333: goto st333;
		case 1673: goto st1673;
		case 334: goto st334;
		case 335: goto st335;
		case 336: goto st336;
		case 337: goto st337;
		case 338: goto st338;
		case 339: goto st339;
		case 340: goto st340;
		case 341: goto st341;
		case 342: goto st342;
		case 343: goto st343;
		case 344: goto st344;
		case 345: goto st345;
		case 346: goto st346;
		case 347: goto st347;
		case 348: goto st348;
		case 349: goto st349;
		case 350: goto st350;
		case 351: goto st351;
		case 352: goto st352;
		case 353: goto st353;
		case 354: goto st354;
		case 355: goto st355;
		case 356: goto st356;
		case 357: goto st357;
		case 358: goto st358;
		case 359: goto st359;
		case 360: goto st360;
		case 361: goto st361;
		case 362: goto st362;
		case 363: goto st363;
		case 364: goto st364;
		case 365: goto st365;
		case 366: goto st366;
		case 367: goto st367;
		case 368: goto st368;
		case 369: goto st369;
		case 370: goto st370;
		case 371: goto st371;
		case 372: goto st372;
		case 373: goto st373;
		case 374: goto st374;
		case 375: goto st375;
		case 376: goto st376;
		case 377: goto st377;
		case 378: goto st378;
		case 379: goto st379;
		case 380: goto st380;
		case 381: goto st381;
		case 382: goto st382;
		case 1674: goto st1674;
		case 383: goto st383;
		case 384: goto st384;
		case 385: goto st385;
		case 1675: goto st1675;
		case 386: goto st386;
		case 387: goto st387;
		case 388: goto st388;
		case 389: goto st389;
		case 390: goto st390;
		case 391: goto st391;
		case 392: goto st392;
		case 393: goto st393;
		case 394: goto st394;
		case 395: goto st395;
		case 396: goto st396;
		case 1676: goto st1676;
		case 397: goto st397;
		case 398: goto st398;
		case 399: goto st399;
		case 400: goto st400;
		case 401: goto st401;
		case 402: goto st402;
		case 403: goto st403;
		case 404: goto st404;
		case 405: goto st405;
		case 406: goto st406;
		case 407: goto st407;
		case 408: goto st408;
		case 409: goto st409;
		case 1677: goto st1677;
		case 410: goto st410;
		case 411: goto st411;
		case 412: goto st412;
		case 413: goto st413;
		case 414: goto st414;
		case 415: goto st415;
		case 416: goto st416;
		case 417: goto st417;
		case 418: goto st418;
		case 419: goto st419;
		case 420: goto st420;
		case 421: goto st421;
		case 422: goto st422;
		case 423: goto st423;
		case 424: goto st424;
		case 425: goto st425;
		case 426: goto st426;
		case 427: goto st427;
		case 428: goto st428;
		case 429: goto st429;
		case 430: goto st430;
		case 431: goto st431;
		case 1678: goto st1678;
		case 432: goto st432;
		case 433: goto st433;
		case 434: goto st434;
		case 435: goto st435;
		case 436: goto st436;
		case 437: goto st437;
		case 438: goto st438;
		case 439: goto st439;
		case 440: goto st440;
		case 441: goto st441;
		case 442: goto st442;
		case 1679: goto st1679;
		case 443: goto st443;
		case 444: goto st444;
		case 445: goto st445;
		case 446: goto st446;
		case 447: goto st447;
		case 448: goto st448;
		case 449: goto st449;
		case 450: goto st450;
		case 451: goto st451;
		case 452: goto st452;
		case 453: goto st453;
		case 1680: goto st1680;
		case 454: goto st454;
		case 455: goto st455;
		case 456: goto st456;
		case 457: goto st457;
		case 458: goto st458;
		case 459: goto st459;
		case 460: goto st460;
		case 461: goto st461;
		case 462: goto st462;
		case 463: goto st463;
		case 464: goto st464;
		case 465: goto st465;
		case 466: goto st466;
		case 467: goto st467;
		case 468: goto st468;
		case 469: goto st469;
		case 470: goto st470;
		case 471: goto st471;
		case 472: goto st472;
		case 473: goto st473;
		case 474: goto st474;
		case 475: goto st475;
		case 476: goto st476;
		case 477: goto st477;
		case 478: goto st478;
		case 479: goto st479;
		case 480: goto st480;
		case 481: goto st481;
		case 482: goto st482;
		case 483: goto st483;
		case 484: goto st484;
		case 485: goto st485;
		case 486: goto st486;
		case 487: goto st487;
		case 488: goto st488;
		case 489: goto st489;
		case 490: goto st490;
		case 491: goto st491;
		case 492: goto st492;
		case 493: goto st493;
		case 494: goto st494;
		case 495: goto st495;
		case 496: goto st496;
		case 497: goto st497;
		case 498: goto st498;
		case 499: goto st499;
		case 500: goto st500;
		case 1681: goto st1681;
		case 501: goto st501;
		case 502: goto st502;
		case 503: goto st503;
		case 504: goto st504;
		case 505: goto st505;
		case 506: goto st506;
		case 507: goto st507;
		case 508: goto st508;
		case 509: goto st509;
		case 1682: goto st1682;
		case 1683: goto st1683;
		case 510: goto st510;
		case 511: goto st511;
		case 512: goto st512;
		case 513: goto st513;
		case 1684: goto st1684;
		case 1685: goto st1685;
		case 514: goto st514;
		case 515: goto st515;
		case 516: goto st516;
		case 517: goto st517;
		case 518: goto st518;
		case 519: goto st519;
		case 520: goto st520;
		case 521: goto st521;
		case 522: goto st522;
		case 1686: goto st1686;
		case 1687: goto st1687;
		case 523: goto st523;
		case 524: goto st524;
		case 525: goto st525;
		case 526: goto st526;
		case 527: goto st527;
		case 528: goto st528;
		case 529: goto st529;
		case 530: goto st530;
		case 531: goto st531;
		case 532: goto st532;
		case 533: goto st533;
		case 534: goto st534;
		case 535: goto st535;
		case 536: goto st536;
		case 537: goto st537;
		case 538: goto st538;
		case 539: goto st539;
		case 540: goto st540;
		case 541: goto st541;
		case 542: goto st542;
		case 543: goto st543;
		case 544: goto st544;
		case 545: goto st545;
		case 546: goto st546;
		case 547: goto st547;
		case 548: goto st548;
		case 549: goto st549;
		case 550: goto st550;
		case 551: goto st551;
		case 552: goto st552;
		case 553: goto st553;
		case 554: goto st554;
		case 555: goto st555;
		case 556: goto st556;
		case 557: goto st557;
		case 558: goto st558;
		case 559: goto st559;
		case 560: goto st560;
		case 561: goto st561;
		case 562: goto st562;
		case 563: goto st563;
		case 564: goto st564;
		case 565: goto st565;
		case 1688: goto st1688;
		case 1689: goto st1689;
		case 566: goto st566;
		case 1690: goto st1690;
		case 1691: goto st1691;
		case 567: goto st567;
		case 568: goto st568;
		case 569: goto st569;
		case 570: goto st570;
		case 571: goto st571;
		case 572: goto st572;
		case 573: goto st573;
		case 574: goto st574;
		case 1692: goto st1692;
		case 1693: goto st1693;
		case 575: goto st575;
		case 1694: goto st1694;
		case 576: goto st576;
		case 577: goto st577;
		case 578: goto st578;
		case 579: goto st579;
		case 580: goto st580;
		case 581: goto st581;
		case 582: goto st582;
		case 583: goto st583;
		case 584: goto st584;
		case 585: goto st585;
		case 586: goto st586;
		case 587: goto st587;
		case 588: goto st588;
		case 589: goto st589;
		case 590: goto st590;
		case 591: goto st591;
		case 592: goto st592;
		case 593: goto st593;
		case 1695: goto st1695;
		case 594: goto st594;
		case 595: goto st595;
		case 596: goto st596;
		case 597: goto st597;
		case 598: goto st598;
		case 599: goto st599;
		case 600: goto st600;
		case 601: goto st601;
		case 602: goto st602;
		case 1696: goto st1696;
		case 1697: goto st1697;
		case 1698: goto st1698;
		case 1699: goto st1699;
		case 1700: goto st1700;
		case 603: goto st603;
		case 604: goto st604;
		case 1701: goto st1701;
		case 1702: goto st1702;
		case 1703: goto st1703;
		case 1704: goto st1704;
		case 1705: goto st1705;
		case 1706: goto st1706;
		case 605: goto st605;
		case 606: goto st606;
		case 1707: goto st1707;
		case 607: goto st607;
		case 608: goto st608;
		case 609: goto st609;
		case 610: goto st610;
		case 611: goto st611;
		case 612: goto st612;
		case 613: goto st613;
		case 614: goto st614;
		case 615: goto st615;
		case 1708: goto st1708;
		case 1709: goto st1709;
		case 1710: goto st1710;
		case 1711: goto st1711;
		case 1712: goto st1712;
		case 616: goto st616;
		case 617: goto st617;
		case 618: goto st618;
		case 619: goto st619;
		case 620: goto st620;
		case 621: goto st621;
		case 622: goto st622;
		case 623: goto st623;
		case 624: goto st624;
		case 625: goto st625;
		case 1713: goto st1713;
		case 1714: goto st1714;
		case 1715: goto st1715;
		case 1716: goto st1716;
		case 626: goto st626;
		case 627: goto st627;
		case 1717: goto st1717;
		case 1718: goto st1718;
		case 1719: goto st1719;
		case 628: goto st628;
		case 629: goto st629;
		case 1720: goto st1720;
		case 1721: goto st1721;
		case 1722: goto st1722;
		case 1723: goto st1723;
		case 1724: goto st1724;
		case 1725: goto st1725;
		case 1726: goto st1726;
		case 1727: goto st1727;
		case 630: goto st630;
		case 631: goto st631;
		case 1728: goto st1728;
		case 1729: goto st1729;
		case 1730: goto st1730;
		case 632: goto st632;
		case 633: goto st633;
		case 1731: goto st1731;
		case 1732: goto st1732;
		case 1733: goto st1733;
		case 1734: goto st1734;
		case 1735: goto st1735;
		case 1736: goto st1736;
		case 634: goto st634;
		case 635: goto st635;
		case 1737: goto st1737;
		case 636: goto st636;
		case 1738: goto st1738;
		case 1739: goto st1739;
		case 1740: goto st1740;
		case 637: goto st637;
		case 638: goto st638;
		case 1741: goto st1741;
		case 1742: goto st1742;
		case 1743: goto st1743;
		case 1744: goto st1744;
		case 1745: goto st1745;
		case 639: goto st639;
		case 640: goto st640;
		case 1746: goto st1746;
		case 1747: goto st1747;
		case 1748: goto st1748;
		case 1749: goto st1749;
		case 1750: goto st1750;
		case 641: goto st641;
		case 642: goto st642;
		case 643: goto st643;
		case 1751: goto st1751;
		case 644: goto st644;
		case 645: goto st645;
		case 646: goto st646;
		case 647: goto st647;
		case 648: goto st648;
		case 649: goto st649;
		case 650: goto st650;
		case 651: goto st651;
		case 652: goto st652;
		case 653: goto st653;
		case 654: goto st654;
		case 1752: goto st1752;
		case 655: goto st655;
		case 656: goto st656;
		case 1753: goto st1753;
		case 1754: goto st1754;
		case 1755: goto st1755;
		case 1756: goto st1756;
		case 1757: goto st1757;
		case 1758: goto st1758;
		case 1759: goto st1759;
		case 1760: goto st1760;
		case 1761: goto st1761;
		case 657: goto st657;
		case 658: goto st658;
		case 659: goto st659;
		case 660: goto st660;
		case 661: goto st661;
		case 662: goto st662;
		case 663: goto st663;
		case 664: goto st664;
		case 665: goto st665;
		case 1762: goto st1762;
		case 666: goto st666;
		case 667: goto st667;
		case 668: goto st668;
		case 669: goto st669;
		case 670: goto st670;
		case 671: goto st671;
		case 672: goto st672;
		case 673: goto st673;
		case 674: goto st674;
		case 675: goto st675;
		case 1763: goto st1763;
		case 676: goto st676;
		case 677: goto st677;
		case 678: goto st678;
		case 679: goto st679;
		case 680: goto st680;
		case 681: goto st681;
		case 682: goto st682;
		case 683: goto st683;
		case 684: goto st684;
		case 685: goto st685;
		case 686: goto st686;
		case 1764: goto st1764;
		case 687: goto st687;
		case 688: goto st688;
		case 689: goto st689;
		case 690: goto st690;
		case 691: goto st691;
		case 692: goto st692;
		case 693: goto st693;
		case 694: goto st694;
		case 695: goto st695;
		case 696: goto st696;
		case 697: goto st697;
		case 698: goto st698;
		case 699: goto st699;
		case 1765: goto st1765;
		case 700: goto st700;
		case 701: goto st701;
		case 702: goto st702;
		case 703: goto st703;
		case 704: goto st704;
		case 705: goto st705;
		case 706: goto st706;
		case 707: goto st707;
		case 708: goto st708;
		case 709: goto st709;
		case 1766: goto st1766;
		case 1767: goto st1767;
		case 1768: goto st1768;
		case 1769: goto st1769;
		case 1770: goto st1770;
		case 1771: goto st1771;
		case 1772: goto st1772;
		case 1773: goto st1773;
		case 1774: goto st1774;
		case 1775: goto st1775;
		case 1776: goto st1776;
		case 1777: goto st1777;
		case 1778: goto st1778;
		case 710: goto st710;
		case 711: goto st711;
		case 1779: goto st1779;
		case 1780: goto st1780;
		case 1781: goto st1781;
		case 1782: goto st1782;
		case 1783: goto st1783;
		case 712: goto st712;
		case 713: goto st713;
		case 1784: goto st1784;
		case 1785: goto st1785;
		case 1786: goto st1786;
		case 1787: goto st1787;
		case 714: goto st714;
		case 715: goto st715;
		case 716: goto st716;
		case 717: goto st717;
		case 718: goto st718;
		case 719: goto st719;
		case 720: goto st720;
		case 721: goto st721;
		case 722: goto st722;
		case 1788: goto st1788;
		case 1789: goto st1789;
		case 1790: goto st1790;
		case 1791: goto st1791;
		case 1792: goto st1792;
		case 723: goto st723;
		case 724: goto st724;
		case 1793: goto st1793;
		case 1794: goto st1794;
		case 1795: goto st1795;
		case 1796: goto st1796;
		case 1797: goto st1797;
		case 725: goto st725;
		case 726: goto st726;
		case 1798: goto st1798;
		case 1799: goto st1799;
		case 1800: goto st1800;
		case 727: goto st727;
		case 728: goto st728;
		case 1801: goto st1801;
		case 729: goto st729;
		case 730: goto st730;
		case 731: goto st731;
		case 732: goto st732;
		case 733: goto st733;
		case 734: goto st734;
		case 735: goto st735;
		case 736: goto st736;
		case 737: goto st737;
		case 1802: goto st1802;
		case 1803: goto st1803;
		case 1804: goto st1804;
		case 1805: goto st1805;
		case 738: goto st738;
		case 739: goto st739;
		case 1806: goto st1806;
		case 1807: goto st1807;
		case 1808: goto st1808;
		case 1809: goto st1809;
		case 1810: goto st1810;
		case 1811: goto st1811;
		case 1812: goto st1812;
		case 740: goto st740;
		case 741: goto st741;
		case 1813: goto st1813;
		case 1814: goto st1814;
		case 1815: goto st1815;
		case 1816: goto st1816;
		case 742: goto st742;
		case 743: goto st743;
		case 1817: goto st1817;
		case 1818: goto st1818;
		case 1819: goto st1819;
		case 1820: goto st1820;
		case 1821: goto st1821;
		case 744: goto st744;
		case 745: goto st745;
		case 746: goto st746;
		case 747: goto st747;
		case 748: goto st748;
		case 749: goto st749;
		case 750: goto st750;
		case 1822: goto st1822;
		case 751: goto st751;
		case 752: goto st752;
		case 753: goto st753;
		case 754: goto st754;
		case 755: goto st755;
		case 756: goto st756;
		case 757: goto st757;
		case 758: goto st758;
		case 1823: goto st1823;
		case 1824: goto st1824;
		case 1825: goto st1825;
		case 1826: goto st1826;
		case 1827: goto st1827;
		case 1828: goto st1828;
		case 1829: goto st1829;
		case 1830: goto st1830;
		case 759: goto st759;
		case 760: goto st760;
		case 1831: goto st1831;
		case 1832: goto st1832;
		case 1833: goto st1833;
		case 1834: goto st1834;
		case 1835: goto st1835;
		case 1836: goto st1836;
		case 761: goto st761;
		case 762: goto st762;
		case 1837: goto st1837;
		case 1838: goto st1838;
		case 1839: goto st1839;
		case 1840: goto st1840;
		case 1841: goto st1841;
		case 1842: goto st1842;
		case 1843: goto st1843;
		case 1844: goto st1844;
		case 1845: goto st1845;
		case 763: goto st763;
		case 764: goto st764;
		case 1846: goto st1846;
		case 1847: goto st1847;
		case 1848: goto st1848;
		case 1849: goto st1849;
		case 1850: goto st1850;
		case 765: goto st765;
		case 766: goto st766;
		case 767: goto st767;
		case 1851: goto st1851;
		case 768: goto st768;
		case 769: goto st769;
		case 770: goto st770;
		case 771: goto st771;
		case 772: goto st772;
		case 773: goto st773;
		case 774: goto st774;
		case 775: goto st775;
		case 776: goto st776;
		case 1852: goto st1852;
		case 777: goto st777;
		case 778: goto st778;
		case 779: goto st779;
		case 780: goto st780;
		case 781: goto st781;
		case 782: goto st782;
		case 1853: goto st1853;
		case 1854: goto st1854;
		case 1855: goto st1855;
		case 1856: goto st1856;
		case 1857: goto st1857;
		case 1858: goto st1858;
		case 1859: goto st1859;
		case 1860: goto st1860;
		case 1861: goto st1861;
		case 1862: goto st1862;
		case 1863: goto st1863;
		case 1864: goto st1864;
		case 1865: goto st1865;
		case 783: goto st783;
		case 784: goto st784;
		case 785: goto st785;
		case 786: goto st786;
		case 787: goto st787;
		case 788: goto st788;
		case 789: goto st789;
		case 790: goto st790;
		case 791: goto st791;
		case 792: goto st792;
		case 793: goto st793;
		case 794: goto st794;
		case 795: goto st795;
		case 796: goto st796;
		case 797: goto st797;
		case 798: goto st798;
		case 799: goto st799;
		case 800: goto st800;
		case 801: goto st801;
		case 802: goto st802;
		case 803: goto st803;
		case 804: goto st804;
		case 805: goto st805;
		case 806: goto st806;
		case 1866: goto st1866;
		case 807: goto st807;
		case 808: goto st808;
		case 809: goto st809;
		case 810: goto st810;
		case 811: goto st811;
		case 812: goto st812;
		case 813: goto st813;
		case 814: goto st814;
		case 815: goto st815;
		case 816: goto st816;
		case 817: goto st817;
		case 818: goto st818;
		case 819: goto st819;
		case 820: goto st820;
		case 1867: goto st1867;
		case 821: goto st821;
		case 822: goto st822;
		case 823: goto st823;
		case 824: goto st824;
		case 825: goto st825;
		case 826: goto st826;
		case 827: goto st827;
		case 828: goto st828;
		case 829: goto st829;
		case 830: goto st830;
		case 831: goto st831;
		case 832: goto st832;
		case 833: goto st833;
		case 834: goto st834;
		case 835: goto st835;
		case 836: goto st836;
		case 837: goto st837;
		case 838: goto st838;
		case 839: goto st839;
		case 840: goto st840;
		case 841: goto st841;
		case 842: goto st842;
		case 843: goto st843;
		case 844: goto st844;
		case 845: goto st845;
		case 846: goto st846;
		case 847: goto st847;
		case 848: goto st848;
		case 849: goto st849;
		case 850: goto st850;
		case 851: goto st851;
		case 852: goto st852;
		case 853: goto st853;
		case 854: goto st854;
		case 855: goto st855;
		case 856: goto st856;
		case 857: goto st857;
		case 858: goto st858;
		case 859: goto st859;
		case 860: goto st860;
		case 861: goto st861;
		case 862: goto st862;
		case 863: goto st863;
		case 864: goto st864;
		case 865: goto st865;
		case 866: goto st866;
		case 867: goto st867;
		case 868: goto st868;
		case 869: goto st869;
		case 1868: goto st1868;
		case 870: goto st870;
		case 1869: goto st1869;
		case 871: goto st871;
		case 872: goto st872;
		case 873: goto st873;
		case 874: goto st874;
		case 875: goto st875;
		case 876: goto st876;
		case 877: goto st877;
		case 878: goto st878;
		case 879: goto st879;
		case 880: goto st880;
		case 881: goto st881;
		case 882: goto st882;
		case 883: goto st883;
		case 884: goto st884;
		case 885: goto st885;
		case 886: goto st886;
		case 887: goto st887;
		case 1870: goto st1870;
		case 888: goto st888;
		case 889: goto st889;
		case 890: goto st890;
		case 891: goto st891;
		case 892: goto st892;
		case 893: goto st893;
		case 894: goto st894;
		case 895: goto st895;
		case 896: goto st896;
		case 897: goto st897;
		case 898: goto st898;
		case 899: goto st899;
		case 900: goto st900;
		case 901: goto st901;
		case 902: goto st902;
		case 903: goto st903;
		case 904: goto st904;
		case 905: goto st905;
		case 906: goto st906;
		case 907: goto st907;
		case 908: goto st908;
		case 909: goto st909;
		case 910: goto st910;
		case 911: goto st911;
		case 912: goto st912;
		case 913: goto st913;
		case 914: goto st914;
		case 915: goto st915;
		case 916: goto st916;
		case 917: goto st917;
		case 918: goto st918;
		case 919: goto st919;
		case 920: goto st920;
		case 921: goto st921;
		case 922: goto st922;
		case 923: goto st923;
		case 924: goto st924;
		case 925: goto st925;
		case 926: goto st926;
		case 927: goto st927;
		case 928: goto st928;
		case 929: goto st929;
		case 930: goto st930;
		case 931: goto st931;
		case 932: goto st932;
		case 933: goto st933;
		case 934: goto st934;
		case 935: goto st935;
		case 936: goto st936;
		case 937: goto st937;
		case 938: goto st938;
		case 939: goto st939;
		case 940: goto st940;
		case 941: goto st941;
		case 942: goto st942;
		case 943: goto st943;
		case 944: goto st944;
		case 945: goto st945;
		case 946: goto st946;
		case 947: goto st947;
		case 948: goto st948;
		case 949: goto st949;
		case 950: goto st950;
		case 951: goto st951;
		case 952: goto st952;
		case 953: goto st953;
		case 954: goto st954;
		case 955: goto st955;
		case 956: goto st956;
		case 957: goto st957;
		case 958: goto st958;
		case 959: goto st959;
		case 960: goto st960;
		case 961: goto st961;
		case 962: goto st962;
		case 963: goto st963;
		case 964: goto st964;
		case 965: goto st965;
		case 966: goto st966;
		case 967: goto st967;
		case 968: goto st968;
		case 969: goto st969;
		case 970: goto st970;
		case 971: goto st971;
		case 972: goto st972;
		case 973: goto st973;
		case 974: goto st974;
		case 975: goto st975;
		case 976: goto st976;
		case 977: goto st977;
		case 978: goto st978;
		case 979: goto st979;
		case 980: goto st980;
		case 981: goto st981;
		case 982: goto st982;
		case 983: goto st983;
		case 984: goto st984;
		case 985: goto st985;
		case 986: goto st986;
		case 987: goto st987;
		case 988: goto st988;
		case 989: goto st989;
		case 990: goto st990;
		case 991: goto st991;
		case 992: goto st992;
		case 993: goto st993;
		case 994: goto st994;
		case 995: goto st995;
		case 996: goto st996;
		case 997: goto st997;
		case 998: goto st998;
		case 999: goto st999;
		case 1000: goto st1000;
		case 1001: goto st1001;
		case 1002: goto st1002;
		case 1003: goto st1003;
		case 1004: goto st1004;
		case 1005: goto st1005;
		case 1006: goto st1006;
		case 1007: goto st1007;
		case 1008: goto st1008;
		case 1009: goto st1009;
		case 1010: goto st1010;
		case 1011: goto st1011;
		case 1012: goto st1012;
		case 1013: goto st1013;
		case 1014: goto st1014;
		case 1015: goto st1015;
		case 1016: goto st1016;
		case 1017: goto st1017;
		case 1018: goto st1018;
		case 1019: goto st1019;
		case 1020: goto st1020;
		case 1021: goto st1021;
		case 1022: goto st1022;
		case 1023: goto st1023;
		case 1024: goto st1024;
		case 1025: goto st1025;
		case 1026: goto st1026;
		case 1027: goto st1027;
		case 1028: goto st1028;
		case 1029: goto st1029;
		case 1030: goto st1030;
		case 1031: goto st1031;
		case 1032: goto st1032;
		case 1033: goto st1033;
		case 1034: goto st1034;
		case 1035: goto st1035;
		case 1036: goto st1036;
		case 1037: goto st1037;
		case 1038: goto st1038;
		case 1039: goto st1039;
		case 1040: goto st1040;
		case 1041: goto st1041;
		case 1042: goto st1042;
		case 1043: goto st1043;
		case 1044: goto st1044;
		case 1045: goto st1045;
		case 1046: goto st1046;
		case 1047: goto st1047;
		case 1048: goto st1048;
		case 1871: goto st1871;
		case 1049: goto st1049;
		case 1050: goto st1050;
		case 1872: goto st1872;
		case 1051: goto st1051;
		case 1052: goto st1052;
		case 1053: goto st1053;
		case 1054: goto st1054;
		case 1055: goto st1055;
		case 1056: goto st1056;
		case 1057: goto st1057;
		case 1058: goto st1058;
		case 1059: goto st1059;
		case 1060: goto st1060;
		case 1061: goto st1061;
		case 1062: goto st1062;
		case 1063: goto st1063;
		case 1064: goto st1064;
		case 1065: goto st1065;
		case 1066: goto st1066;
		case 1067: goto st1067;
		case 1873: goto st1873;
		case 1068: goto st1068;
		case 1069: goto st1069;
		case 1070: goto st1070;
		case 1071: goto st1071;
		case 1072: goto st1072;
		case 1073: goto st1073;
		case 1074: goto st1074;
		case 1075: goto st1075;
		case 1076: goto st1076;
		case 1077: goto st1077;
		case 1078: goto st1078;
		case 1079: goto st1079;
		case 1080: goto st1080;
		case 1081: goto st1081;
		case 1082: goto st1082;
		case 1083: goto st1083;
		case 1084: goto st1084;
		case 1085: goto st1085;
		case 1086: goto st1086;
		case 1087: goto st1087;
		case 1088: goto st1088;
		case 1089: goto st1089;
		case 1090: goto st1090;
		case 1091: goto st1091;
		case 1092: goto st1092;
		case 1093: goto st1093;
		case 1094: goto st1094;
		case 1095: goto st1095;
		case 1096: goto st1096;
		case 1097: goto st1097;
		case 1098: goto st1098;
		case 1099: goto st1099;
		case 1100: goto st1100;
		case 1101: goto st1101;
		case 1102: goto st1102;
		case 1103: goto st1103;
		case 1104: goto st1104;
		case 1105: goto st1105;
		case 1106: goto st1106;
		case 1107: goto st1107;
		case 1108: goto st1108;
		case 1109: goto st1109;
		case 1110: goto st1110;
		case 1874: goto st1874;
		case 1111: goto st1111;
		case 1112: goto st1112;
		case 1113: goto st1113;
		case 1114: goto st1114;
		case 1115: goto st1115;
		case 1116: goto st1116;
		case 1117: goto st1117;
		case 1118: goto st1118;
		case 1119: goto st1119;
		case 1120: goto st1120;
		case 1121: goto st1121;
		case 1122: goto st1122;
		case 1123: goto st1123;
		case 1124: goto st1124;
		case 1125: goto st1125;
		case 1126: goto st1126;
		case 1127: goto st1127;
		case 1128: goto st1128;
		case 1129: goto st1129;
		case 1130: goto st1130;
		case 1875: goto st1875;
		case 1131: goto st1131;
		case 1132: goto st1132;
		case 1133: goto st1133;
		case 1134: goto st1134;
		case 1135: goto st1135;
		case 1136: goto st1136;
		case 1137: goto st1137;
		case 1138: goto st1138;
		case 1139: goto st1139;
		case 1140: goto st1140;
		case 1141: goto st1141;
		case 1142: goto st1142;
		case 1143: goto st1143;
		case 1144: goto st1144;
		case 1145: goto st1145;
		case 1146: goto st1146;
		case 1147: goto st1147;
		case 1148: goto st1148;
		case 1149: goto st1149;
		case 1150: goto st1150;
		case 1151: goto st1151;
		case 1152: goto st1152;
		case 1153: goto st1153;
		case 1876: goto st1876;
		case 1154: goto st1154;
		case 1155: goto st1155;
		case 1156: goto st1156;
		case 1157: goto st1157;
		case 1158: goto st1158;
		case 1159: goto st1159;
		case 1160: goto st1160;
		case 1161: goto st1161;
		case 1162: goto st1162;
		case 1163: goto st1163;
		case 1164: goto st1164;
		case 1165: goto st1165;
		case 1166: goto st1166;
		case 1167: goto st1167;
		case 1168: goto st1168;
		case 1169: goto st1169;
		case 1170: goto st1170;
		case 1171: goto st1171;
		case 1172: goto st1172;
		case 1173: goto st1173;
		case 1174: goto st1174;
		case 1175: goto st1175;
		case 1176: goto st1176;
		case 1177: goto st1177;
		case 1178: goto st1178;
		case 1179: goto st1179;
		case 1180: goto st1180;
		case 1877: goto st1877;
		case 1181: goto st1181;
		case 1182: goto st1182;
		case 1183: goto st1183;
		case 1184: goto st1184;
		case 1185: goto st1185;
		case 1186: goto st1186;
		case 1187: goto st1187;
		case 1188: goto st1188;
		case 1189: goto st1189;
		case 1190: goto st1190;
		case 1191: goto st1191;
		case 1192: goto st1192;
		case 1193: goto st1193;
		case 1194: goto st1194;
		case 1195: goto st1195;
		case 1196: goto st1196;
		case 1197: goto st1197;
		case 1198: goto st1198;
		case 1199: goto st1199;
		case 1200: goto st1200;
		case 1201: goto st1201;
		case 1202: goto st1202;
		case 1203: goto st1203;
		case 1204: goto st1204;
		case 1205: goto st1205;
		case 1206: goto st1206;
		case 1207: goto st1207;
		case 1208: goto st1208;
		case 1209: goto st1209;
		case 1210: goto st1210;
		case 1211: goto st1211;
		case 1212: goto st1212;
		case 1213: goto st1213;
		case 1214: goto st1214;
		case 1215: goto st1215;
		case 1216: goto st1216;
		case 1217: goto st1217;
		case 1218: goto st1218;
		case 1219: goto st1219;
		case 1220: goto st1220;
		case 1221: goto st1221;
		case 1222: goto st1222;
		case 1223: goto st1223;
		case 1224: goto st1224;
		case 1225: goto st1225;
		case 1226: goto st1226;
		case 1227: goto st1227;
		case 1228: goto st1228;
		case 1229: goto st1229;
		case 1230: goto st1230;
		case 1231: goto st1231;
		case 1232: goto st1232;
		case 1233: goto st1233;
		case 1234: goto st1234;
		case 1235: goto st1235;
		case 1236: goto st1236;
		case 1237: goto st1237;
		case 1238: goto st1238;
		case 1239: goto st1239;
		case 1240: goto st1240;
		case 1241: goto st1241;
		case 1242: goto st1242;
		case 1243: goto st1243;
		case 1244: goto st1244;
		case 1245: goto st1245;
		case 1246: goto st1246;
		case 1247: goto st1247;
		case 1248: goto st1248;
		case 1249: goto st1249;
		case 1250: goto st1250;
		case 1251: goto st1251;
		case 1252: goto st1252;
		case 1253: goto st1253;
		case 1254: goto st1254;
		case 1255: goto st1255;
		case 1256: goto st1256;
		case 1257: goto st1257;
		case 1258: goto st1258;
		case 1259: goto st1259;
		case 1260: goto st1260;
		case 1261: goto st1261;
		case 1262: goto st1262;
		case 1878: goto st1878;
		case 1879: goto st1879;
		case 1263: goto st1263;
		case 1264: goto st1264;
		case 1265: goto st1265;
		case 1266: goto st1266;
		case 1267: goto st1267;
		case 1268: goto st1268;
		case 1269: goto st1269;
		case 1270: goto st1270;
		case 1271: goto st1271;
		case 1272: goto st1272;
		case 1273: goto st1273;
		case 1274: goto st1274;
		case 1275: goto st1275;
		case 1276: goto st1276;
		case 1277: goto st1277;
		case 1278: goto st1278;
		case 1279: goto st1279;
		case 1280: goto st1280;
		case 1281: goto st1281;
		case 1282: goto st1282;
		case 1283: goto st1283;
		case 1284: goto st1284;
		case 1285: goto st1285;
		case 1286: goto st1286;
		case 1287: goto st1287;
		case 1288: goto st1288;
		case 1289: goto st1289;
		case 1290: goto st1290;
		case 1291: goto st1291;
		case 1292: goto st1292;
		case 1293: goto st1293;
		case 1294: goto st1294;
		case 1295: goto st1295;
		case 1296: goto st1296;
		case 1297: goto st1297;
		case 1298: goto st1298;
		case 1299: goto st1299;
		case 1300: goto st1300;
		case 1301: goto st1301;
		case 1302: goto st1302;
		case 1303: goto st1303;
		case 1304: goto st1304;
		case 1305: goto st1305;
		case 1880: goto st1880;
		case 1306: goto st1306;
		case 1307: goto st1307;
		case 1308: goto st1308;
		case 1309: goto st1309;
		case 1310: goto st1310;
		case 1311: goto st1311;
		case 1312: goto st1312;
		case 1313: goto st1313;
		case 1314: goto st1314;
		case 1315: goto st1315;
		case 1316: goto st1316;
		case 1317: goto st1317;
		case 1318: goto st1318;
		case 1319: goto st1319;
		case 1320: goto st1320;
		case 1321: goto st1321;
		case 1322: goto st1322;
		case 1323: goto st1323;
		case 1324: goto st1324;
		case 1325: goto st1325;
		case 1326: goto st1326;
		case 1327: goto st1327;
		case 1328: goto st1328;
		case 1329: goto st1329;
		case 1330: goto st1330;
		case 1331: goto st1331;
		case 1332: goto st1332;
		case 1333: goto st1333;
		case 1334: goto st1334;
		case 1335: goto st1335;
		case 1336: goto st1336;
		case 1337: goto st1337;
		case 1338: goto st1338;
		case 1339: goto st1339;
		case 1881: goto st1881;
		case 1340: goto st1340;
		case 1341: goto st1341;
		case 1882: goto st1882;
		case 1342: goto st1342;
		case 1343: goto st1343;
		case 1344: goto st1344;
		case 1345: goto st1345;
		case 1883: goto st1883;
		case 1346: goto st1346;
		case 1347: goto st1347;
		case 1348: goto st1348;
		case 1349: goto st1349;
		case 1350: goto st1350;
		case 1351: goto st1351;
		case 1352: goto st1352;
		case 1353: goto st1353;
		case 1354: goto st1354;
		case 1355: goto st1355;
		case 1884: goto st1884;
		case 1356: goto st1356;
		case 1357: goto st1357;
		case 1358: goto st1358;
		case 1359: goto st1359;
		case 1360: goto st1360;
		case 1361: goto st1361;
		case 1362: goto st1362;
		case 1363: goto st1363;
		case 1364: goto st1364;
		case 1365: goto st1365;
		case 1366: goto st1366;
		case 1367: goto st1367;
		case 1368: goto st1368;
		case 1369: goto st1369;
		case 1370: goto st1370;
		case 1371: goto st1371;
		case 1372: goto st1372;
		case 1373: goto st1373;
		case 1374: goto st1374;
		case 1375: goto st1375;
		case 1885: goto st1885;
		case 1886: goto st1886;
		case 1376: goto st1376;
		case 1377: goto st1377;
		case 1378: goto st1378;
		case 1379: goto st1379;
		case 1380: goto st1380;
		case 1381: goto st1381;
		case 1382: goto st1382;
		case 1383: goto st1383;
		case 1384: goto st1384;
		case 1385: goto st1385;
		case 1386: goto st1386;
		case 1387: goto st1387;
		case 1887: goto st1887;
		case 1888: goto st1888;
		case 1889: goto st1889;
		case 1890: goto st1890;
		case 1388: goto st1388;
		case 1389: goto st1389;
		case 1390: goto st1390;
		case 1391: goto st1391;
		case 1392: goto st1392;
		case 1393: goto st1393;
		case 1394: goto st1394;
		case 1395: goto st1395;
		case 1396: goto st1396;
		case 1397: goto st1397;
		case 1398: goto st1398;
		case 1399: goto st1399;
		case 1400: goto st1400;
		case 1401: goto st1401;
		case 1402: goto st1402;
		case 1403: goto st1403;
		case 1404: goto st1404;
		case 1405: goto st1405;
		case 1891: goto st1891;
		case 1892: goto st1892;
		case 1893: goto st1893;
		case 1894: goto st1894;
		case 1406: goto st1406;
		case 1407: goto st1407;
		case 1408: goto st1408;
		case 1409: goto st1409;
		case 1410: goto st1410;
		case 1411: goto st1411;
		case 1412: goto st1412;
		case 1413: goto st1413;
		case 1414: goto st1414;
		case 1415: goto st1415;
		case 1416: goto st1416;
		case 1417: goto st1417;
		case 1418: goto st1418;
		case 1419: goto st1419;
		case 1420: goto st1420;
		case 1421: goto st1421;
		case 1422: goto st1422;
		case 1423: goto st1423;
		case 1424: goto st1424;
		case 1425: goto st1425;
		case 1426: goto st1426;
		case 1427: goto st1427;
		case 1428: goto st1428;
		case 1429: goto st1429;
		case 1430: goto st1430;
		case 1431: goto st1431;
		case 1432: goto st1432;
		case 1433: goto st1433;
		case 1434: goto st1434;
		case 1435: goto st1435;
		case 1436: goto st1436;
		case 1437: goto st1437;
		case 1438: goto st1438;
		case 1439: goto st1439;
		case 1440: goto st1440;
		case 1441: goto st1441;
		case 1442: goto st1442;
		case 1443: goto st1443;
		case 1444: goto st1444;
		case 1445: goto st1445;
		case 1446: goto st1446;
		case 1447: goto st1447;
		case 1448: goto st1448;
		case 1449: goto st1449;
		case 1450: goto st1450;
		case 1451: goto st1451;
		case 1452: goto st1452;
		case 1453: goto st1453;
		case 1454: goto st1454;
		case 1455: goto st1455;
		case 1456: goto st1456;
		case 1457: goto st1457;
		case 1458: goto st1458;
		case 1459: goto st1459;
		case 1460: goto st1460;
		case 1461: goto st1461;
		case 1462: goto st1462;
		case 1463: goto st1463;
		case 1464: goto st1464;
		case 1465: goto st1465;
		case 1466: goto st1466;
		case 1467: goto st1467;
		case 1468: goto st1468;
		case 1469: goto st1469;
		case 1470: goto st1470;
		case 1471: goto st1471;
		case 1472: goto st1472;
		case 1473: goto st1473;
		case 1474: goto st1474;
		case 1475: goto st1475;
		case 1476: goto st1476;
		case 1477: goto st1477;
		case 1478: goto st1478;
		case 1479: goto st1479;
		case 1480: goto st1480;
		case 1481: goto st1481;
		case 1482: goto st1482;
		case 1483: goto st1483;
		case 1484: goto st1484;
		case 1485: goto st1485;
		case 1486: goto st1486;
		case 1487: goto st1487;
		case 1488: goto st1488;
		case 1489: goto st1489;
		case 1490: goto st1490;
		case 1491: goto st1491;
		case 1492: goto st1492;
		case 1493: goto st1493;
		case 1494: goto st1494;
		case 1495: goto st1495;
		case 1496: goto st1496;
		case 1497: goto st1497;
		case 1498: goto st1498;
		case 1499: goto st1499;
		case 1500: goto st1500;
		case 1501: goto st1501;
		case 1502: goto st1502;
		case 1503: goto st1503;
		case 1504: goto st1504;
		case 1505: goto st1505;
		case 1506: goto st1506;
		case 1507: goto st1507;
		case 1508: goto st1508;
		case 1509: goto st1509;
		case 1510: goto st1510;
		case 1511: goto st1511;
		case 1512: goto st1512;
		case 1513: goto st1513;
		case 1514: goto st1514;
		case 1515: goto st1515;
		case 1516: goto st1516;
		case 1517: goto st1517;
		case 1895: goto st1895;
		case 1518: goto st1518;
		case 1519: goto st1519;
		case 1520: goto st1520;
		case 1521: goto st1521;
		case 1522: goto st1522;
		case 1523: goto st1523;
		case 1524: goto st1524;
		case 1525: goto st1525;
		case 1526: goto st1526;
		case 1527: goto st1527;
		case 1528: goto st1528;
		case 1529: goto st1529;
		case 1530: goto st1530;
		case 1531: goto st1531;
		case 1532: goto st1532;
		case 1533: goto st1533;
		case 1534: goto st1534;
		case 1535: goto st1535;
		case 1536: goto st1536;
		case 1537: goto st1537;
		case 1538: goto st1538;
		case 1539: goto st1539;
		case 1540: goto st1540;
		case 1541: goto st1541;
		case 1542: goto st1542;
		case 1543: goto st1543;
		case 1544: goto st1544;
		case 1545: goto st1545;
		case 1546: goto st1546;
		case 1547: goto st1547;
		case 1548: goto st1548;
		case 1549: goto st1549;
		case 1550: goto st1550;
		case 1551: goto st1551;
		case 1552: goto st1552;
		case 1553: goto st1553;
		case 1554: goto st1554;
		case 1555: goto st1555;
		case 1556: goto st1556;
		case 1557: goto st1557;
		case 1558: goto st1558;
		case 1559: goto st1559;
		case 1560: goto st1560;
		case 1561: goto st1561;
		case 1562: goto st1562;
		case 1563: goto st1563;
		case 1564: goto st1564;
		case 1565: goto st1565;
		case 1566: goto st1566;
		case 1567: goto st1567;
		case 1568: goto st1568;
		case 1569: goto st1569;
		case 1570: goto st1570;
		case 1571: goto st1571;
		case 1572: goto st1572;
		case 1573: goto st1573;
		case 1574: goto st1574;
		case 1575: goto st1575;
		case 1576: goto st1576;
		case 1577: goto st1577;
		case 1578: goto st1578;
		case 1579: goto st1579;
		case 1580: goto st1580;
		case 1581: goto st1581;
		case 1582: goto st1582;
		case 1583: goto st1583;
		case 1584: goto st1584;
		case 1585: goto st1585;
		case 1586: goto st1586;
		case 1587: goto st1587;
		case 1588: goto st1588;
		case 1589: goto st1589;
		case 1590: goto st1590;
		case 1591: goto st1591;
		case 1592: goto st1592;
		case 1593: goto st1593;
		case 1594: goto st1594;
		case 1595: goto st1595;
		case 1596: goto st1596;
		case 1597: goto st1597;
		case 1598: goto st1598;
		case 1599: goto st1599;
		case 1600: goto st1600;
		case 1601: goto st1601;
		case 1602: goto st1602;
		case 1603: goto st1603;
		case 1604: goto st1604;
		case 1605: goto st1605;
		case 1606: goto st1606;
		case 1607: goto st1607;
		case 1608: goto st1608;
		case 1609: goto st1609;
		case 1610: goto st1610;
		case 1611: goto st1611;
		case 1612: goto st1612;
		case 1613: goto st1613;
		case 1614: goto st1614;
		case 1615: goto st1615;
		case 1616: goto st1616;
		case 1617: goto st1617;
		case 1618: goto st1618;
		case 1619: goto st1619;
		case 1620: goto st1620;
		case 1621: goto st1621;
		case 1622: goto st1622;
		case 1623: goto st1623;
		case 1624: goto st1624;
		case 1625: goto st1625;
		case 1626: goto st1626;
		case 1627: goto st1627;
		case 1628: goto st1628;
		case 1629: goto st1629;
	default: break;
	}

	if ( ++( p) == ( pe) )
		goto _test_eof;
_resume:
	switch ( ( cs) )
	{
tr0:
#line 1 "NONE"
	{	switch( ( act) ) {
	case 136:
	{{( p) = ((( te)))-1;}
    g_debug("block blank line(s)");
  }
	break;
	case 137:
	{{( p) = ((( te)))-1;}
    g_debug("block char");
    ( p)--;

    if (dstack.empty() || dstack_check(BLOCK_QUOTE) || dstack_check(BLOCK_SPOILER) || dstack_check(BLOCK_EXPAND)) {
      dstack_open_element(BLOCK_P, "<p>");
    }

    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1659;}}
  }
	break;
	default:
	{{( p) = ((( te)))-1;}}
	break;
	}
	}
	goto st1630;
tr3:
#line 776 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    g_debug("block char");
    ( p)--;

    if (dstack.empty() || dstack_check(BLOCK_QUOTE) || dstack_check(BLOCK_SPOILER) || dstack_check(BLOCK_EXPAND)) {
      dstack_open_element(BLOCK_P, "<p>");
    }

    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1659;}}
  }}
	goto st1630;
tr77:
#line 748 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_TABLE, "<table class=\"striped\">");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1893;}}
  }}
	goto st1630;
tr105:
#line 718 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    append_code_fence({ b1, b2 }, { a1, a2 });
  }}
	goto st1630;
tr130:
#line 713 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_block_code({ a1, a2 });
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1885;}}
  }}
	goto st1630;
tr131:
#line 713 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_block_code({ a1, a2 });
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1885;}}
  }}
	goto st1630;
tr133:
#line 708 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_block_code();
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1885;}}
  }}
	goto st1630;
tr134:
#line 708 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_block_code();
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1885;}}
  }}
	goto st1630;
tr157:
#line 742 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_NODTEXT, "<p>");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1889;}}
  }}
	goto st1630;
tr158:
#line 742 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_NODTEXT, "<p>");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1889;}}
  }}
	goto st1630;
tr169:
#line 754 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_TN, "<p class=\"tn\">");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1659;}}
  }}
	goto st1630;
tr2015:
#line 776 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    g_debug("block char");
    ( p)--;

    if (dstack.empty() || dstack_check(BLOCK_QUOTE) || dstack_check(BLOCK_SPOILER) || dstack_check(BLOCK_EXPAND)) {
      dstack_open_element(BLOCK_P, "<p>");
    }

    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1659;}}
  }}
	goto st1630;
tr2026:
#line 776 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("block char");
    ( p)--;

    if (dstack.empty() || dstack_check(BLOCK_QUOTE) || dstack_check(BLOCK_SPOILER) || dstack_check(BLOCK_EXPAND)) {
      dstack_open_element(BLOCK_P, "<p>");
    }

    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1659;}}
  }}
	goto st1630;
tr2027:
#line 695 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_QUOTE);
  }}
	goto st1630;
tr2028:
#line 738 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_EXPAND);
  }}
	goto st1630;
tr2029:
#line 704 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_SPOILER);
  }}
	goto st1630;
tr2030:
#line 760 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    append_block("<hr>");
  }}
	goto st1630;
tr2031:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 765 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_open_list(e2 - e1);
    {( p) = (( f1))-1;}
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1659;}}
  }}
	goto st1630;
tr2034:
#line 685 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_header(*a1, { b1, b2 });
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1659;}}
  }}
	goto st1630;
tr2043:
#line 690 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_QUOTE, "<blockquote>");
  }}
	goto st1630;
tr2044:
#line 713 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_block_code({ a1, a2 });
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1885;}}
  }}
	goto st1630;
tr2045:
#line 708 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_block_code();
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1885;}}
  }}
	goto st1630;
tr2046:
#line 729 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("block [expand=]");
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_EXPAND, "<details>");
    append_block("<summary>");
    append_block_html_escaped({ a1, a2 });
    append_block("</summary><div>");
  }}
	goto st1630;
tr2048:
#line 723 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_EXPAND, "<details>");
    append_block("<summary>Show</summary><div>");
  }}
	goto st1630;
tr2049:
#line 742 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_NODTEXT, "<p>");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1630;goto st1889;}}
  }}
	goto st1630;
tr2050:
#line 699 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_SPOILER, "<div class=\"spoiler\">");
  }}
	goto st1630;
st1630:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1630;
case 1630:
#line 1 "NONE"
	{( ts) = ( p);}
#line 3114 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) > 60 ) {
		if ( 91 <= (*( p)) && (*( p)) <= 91 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 60 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 0: goto tr4;
		case 9: goto tr2016;
		case 10: goto tr6;
		case 32: goto tr2016;
		case 42: goto tr2018;
		case 72: goto tr2020;
		case 96: goto tr2021;
		case 104: goto tr2020;
		case 3388: goto tr2022;
		case 3419: goto tr2023;
		case 3644: goto tr2024;
		case 3675: goto tr2025;
	}
	if ( _widec < 14 ) {
		if ( _widec > 8 ) {
			if ( 11 <= _widec && _widec <= 13 )
				goto tr2017;
		} else
			goto tr2015;
	} else if ( _widec > 59 ) {
		if ( _widec > 90 ) {
			if ( 92 <= _widec )
				goto tr2015;
		} else if ( _widec >= 61 )
			goto tr2015;
	} else
		goto tr2015;
	goto st0;
tr1:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 772 "ext/dtext/dtext.cpp.rl"
	{( act) = 136;}
	goto st1631;
tr4:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 771 "ext/dtext/dtext.cpp.rl"
	{( act) = 135;}
	goto st1631;
st1631:
	if ( ++( p) == ( pe) )
		goto _test_eof1631;
case 1631:
#line 3174 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1;
		case 9: goto st1;
		case 10: goto tr1;
		case 32: goto st1;
	}
	goto tr0;
st1:
	if ( ++( p) == ( pe) )
		goto _test_eof1;
case 1:
	switch( (*( p)) ) {
		case 0: goto tr1;
		case 9: goto st1;
		case 10: goto tr1;
		case 32: goto st1;
	}
	goto tr0;
tr2016:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 776 "ext/dtext/dtext.cpp.rl"
	{( act) = 137;}
	goto st1632;
st1632:
	if ( ++( p) == ( pe) )
		goto _test_eof1632;
case 1632:
#line 3203 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) > 60 ) {
		if ( 91 <= (*( p)) && (*( p)) <= 91 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 60 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 0: goto tr4;
		case 9: goto st2;
		case 10: goto tr6;
		case 32: goto st2;
		case 3388: goto st56;
		case 3419: goto st65;
		case 3644: goto st73;
		case 3675: goto st74;
	}
	if ( 11 <= _widec && _widec <= 13 )
		goto st4;
	goto tr2026;
st2:
	if ( ++( p) == ( pe) )
		goto _test_eof2;
case 2:
	_widec = (*( p));
	if ( (*( p)) > 60 ) {
		if ( 91 <= (*( p)) && (*( p)) <= 91 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 60 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 0: goto tr4;
		case 9: goto st2;
		case 10: goto tr6;
		case 32: goto st2;
		case 3388: goto st56;
		case 3419: goto st65;
		case 3644: goto st73;
		case 3675: goto st74;
	}
	if ( 11 <= _widec && _widec <= 13 )
		goto st4;
	goto tr3;
tr13:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 772 "ext/dtext/dtext.cpp.rl"
	{( act) = 136;}
	goto st1633;
tr6:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 771 "ext/dtext/dtext.cpp.rl"
	{( act) = 135;}
	goto st1633;
st1633:
	if ( ++( p) == ( pe) )
		goto _test_eof1633;
case 1633:
#line 3278 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) > 60 ) {
		if ( 91 <= (*( p)) && (*( p)) <= 91 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 60 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 0: goto tr1;
		case 9: goto st3;
		case 10: goto tr13;
		case 32: goto st3;
		case 3388: goto st5;
		case 3419: goto st28;
		case 3644: goto st36;
		case 3675: goto st46;
	}
	if ( 11 <= _widec && _widec <= 13 )
		goto st4;
	goto tr0;
st3:
	if ( ++( p) == ( pe) )
		goto _test_eof3;
case 3:
	_widec = (*( p));
	if ( (*( p)) > 60 ) {
		if ( 91 <= (*( p)) && (*( p)) <= 91 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 60 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 0: goto tr1;
		case 9: goto st3;
		case 10: goto tr13;
		case 32: goto st3;
		case 3388: goto st5;
		case 3419: goto st28;
		case 3644: goto st36;
		case 3675: goto st46;
	}
	if ( 11 <= _widec && _widec <= 13 )
		goto st4;
	goto tr0;
st4:
	if ( ++( p) == ( pe) )
		goto _test_eof4;
case 4:
	_widec = (*( p));
	if ( (*( p)) > 60 ) {
		if ( 91 <= (*( p)) && (*( p)) <= 91 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 60 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 32: goto st4;
		case 3388: goto st5;
		case 3419: goto st28;
		case 3644: goto st36;
		case 3675: goto st46;
	}
	if ( 9 <= _widec && _widec <= 13 )
		goto st4;
	goto tr0;
st5:
	if ( ++( p) == ( pe) )
		goto _test_eof5;
case 5:
	if ( (*( p)) == 47 )
		goto st6;
	goto tr0;
st6:
	if ( ++( p) == ( pe) )
		goto _test_eof6;
case 6:
	switch( (*( p)) ) {
		case 66: goto st7;
		case 69: goto st17;
		case 81: goto st23;
		case 98: goto st7;
		case 101: goto st17;
		case 113: goto st23;
	}
	goto tr0;
st7:
	if ( ++( p) == ( pe) )
		goto _test_eof7;
case 7:
	switch( (*( p)) ) {
		case 76: goto st8;
		case 108: goto st8;
	}
	goto tr0;
st8:
	if ( ++( p) == ( pe) )
		goto _test_eof8;
case 8:
	switch( (*( p)) ) {
		case 79: goto st9;
		case 111: goto st9;
	}
	goto tr0;
st9:
	if ( ++( p) == ( pe) )
		goto _test_eof9;
case 9:
	switch( (*( p)) ) {
		case 67: goto st10;
		case 99: goto st10;
	}
	goto tr0;
st10:
	if ( ++( p) == ( pe) )
		goto _test_eof10;
case 10:
	switch( (*( p)) ) {
		case 75: goto st11;
		case 107: goto st11;
	}
	goto tr0;
st11:
	if ( ++( p) == ( pe) )
		goto _test_eof11;
case 11:
	switch( (*( p)) ) {
		case 81: goto st12;
		case 113: goto st12;
	}
	goto tr0;
st12:
	if ( ++( p) == ( pe) )
		goto _test_eof12;
case 12:
	switch( (*( p)) ) {
		case 85: goto st13;
		case 117: goto st13;
	}
	goto tr0;
st13:
	if ( ++( p) == ( pe) )
		goto _test_eof13;
case 13:
	switch( (*( p)) ) {
		case 79: goto st14;
		case 111: goto st14;
	}
	goto tr0;
st14:
	if ( ++( p) == ( pe) )
		goto _test_eof14;
case 14:
	switch( (*( p)) ) {
		case 84: goto st15;
		case 116: goto st15;
	}
	goto tr0;
st15:
	if ( ++( p) == ( pe) )
		goto _test_eof15;
case 15:
	switch( (*( p)) ) {
		case 69: goto st16;
		case 101: goto st16;
	}
	goto tr0;
st16:
	if ( ++( p) == ( pe) )
		goto _test_eof16;
case 16:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2653 )
		goto st1634;
	goto tr0;
st1634:
	if ( ++( p) == ( pe) )
		goto _test_eof1634;
case 1634:
	switch( (*( p)) ) {
		case 9: goto st1634;
		case 32: goto st1634;
	}
	goto tr2027;
st17:
	if ( ++( p) == ( pe) )
		goto _test_eof17;
case 17:
	switch( (*( p)) ) {
		case 88: goto st18;
		case 120: goto st18;
	}
	goto tr0;
st18:
	if ( ++( p) == ( pe) )
		goto _test_eof18;
case 18:
	switch( (*( p)) ) {
		case 80: goto st19;
		case 112: goto st19;
	}
	goto tr0;
st19:
	if ( ++( p) == ( pe) )
		goto _test_eof19;
case 19:
	switch( (*( p)) ) {
		case 65: goto st20;
		case 97: goto st20;
	}
	goto tr0;
st20:
	if ( ++( p) == ( pe) )
		goto _test_eof20;
case 20:
	switch( (*( p)) ) {
		case 78: goto st21;
		case 110: goto st21;
	}
	goto tr0;
st21:
	if ( ++( p) == ( pe) )
		goto _test_eof21;
case 21:
	switch( (*( p)) ) {
		case 68: goto st22;
		case 100: goto st22;
	}
	goto tr0;
st22:
	if ( ++( p) == ( pe) )
		goto _test_eof22;
case 22:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_EXPAND)  ) _widec += 256;
	}
	if ( _widec == 3134 )
		goto st1635;
	goto tr0;
st1635:
	if ( ++( p) == ( pe) )
		goto _test_eof1635;
case 1635:
	switch( (*( p)) ) {
		case 9: goto st1635;
		case 32: goto st1635;
	}
	goto tr2028;
st23:
	if ( ++( p) == ( pe) )
		goto _test_eof23;
case 23:
	switch( (*( p)) ) {
		case 85: goto st24;
		case 117: goto st24;
	}
	goto tr0;
st24:
	if ( ++( p) == ( pe) )
		goto _test_eof24;
case 24:
	switch( (*( p)) ) {
		case 79: goto st25;
		case 111: goto st25;
	}
	goto tr0;
st25:
	if ( ++( p) == ( pe) )
		goto _test_eof25;
case 25:
	switch( (*( p)) ) {
		case 84: goto st26;
		case 116: goto st26;
	}
	goto tr0;
st26:
	if ( ++( p) == ( pe) )
		goto _test_eof26;
case 26:
	switch( (*( p)) ) {
		case 69: goto st27;
		case 101: goto st27;
	}
	goto tr0;
st27:
	if ( ++( p) == ( pe) )
		goto _test_eof27;
case 27:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2622 )
		goto st1634;
	goto tr0;
st28:
	if ( ++( p) == ( pe) )
		goto _test_eof28;
case 28:
	if ( (*( p)) == 47 )
		goto st29;
	goto tr0;
st29:
	if ( ++( p) == ( pe) )
		goto _test_eof29;
case 29:
	switch( (*( p)) ) {
		case 69: goto st30;
		case 81: goto st12;
		case 101: goto st30;
		case 113: goto st12;
	}
	goto tr0;
st30:
	if ( ++( p) == ( pe) )
		goto _test_eof30;
case 30:
	switch( (*( p)) ) {
		case 88: goto st31;
		case 120: goto st31;
	}
	goto tr0;
st31:
	if ( ++( p) == ( pe) )
		goto _test_eof31;
case 31:
	switch( (*( p)) ) {
		case 80: goto st32;
		case 112: goto st32;
	}
	goto tr0;
st32:
	if ( ++( p) == ( pe) )
		goto _test_eof32;
case 32:
	switch( (*( p)) ) {
		case 65: goto st33;
		case 97: goto st33;
	}
	goto tr0;
st33:
	if ( ++( p) == ( pe) )
		goto _test_eof33;
case 33:
	switch( (*( p)) ) {
		case 78: goto st34;
		case 110: goto st34;
	}
	goto tr0;
st34:
	if ( ++( p) == ( pe) )
		goto _test_eof34;
case 34:
	switch( (*( p)) ) {
		case 68: goto st35;
		case 100: goto st35;
	}
	goto tr0;
st35:
	if ( ++( p) == ( pe) )
		goto _test_eof35;
case 35:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_EXPAND)  ) _widec += 256;
	}
	if ( _widec == 3165 )
		goto st1635;
	goto tr0;
st36:
	if ( ++( p) == ( pe) )
		goto _test_eof36;
case 36:
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3375: goto st6;
		case 3631: goto st37;
	}
	goto tr0;
st37:
	if ( ++( p) == ( pe) )
		goto _test_eof37;
case 37:
	_widec = (*( p));
	if ( (*( p)) > 83 ) {
		if ( 115 <= (*( p)) && (*( p)) <= 115 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 83 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 66: goto st7;
		case 69: goto st17;
		case 81: goto st23;
		case 98: goto st7;
		case 101: goto st17;
		case 113: goto st23;
		case 3667: goto st38;
		case 3699: goto st38;
	}
	goto tr0;
st38:
	if ( ++( p) == ( pe) )
		goto _test_eof38;
case 38:
	_widec = (*( p));
	if ( (*( p)) > 80 ) {
		if ( 112 <= (*( p)) && (*( p)) <= 112 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 80 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3664: goto st39;
		case 3696: goto st39;
	}
	goto tr0;
st39:
	if ( ++( p) == ( pe) )
		goto _test_eof39;
case 39:
	_widec = (*( p));
	if ( (*( p)) > 79 ) {
		if ( 111 <= (*( p)) && (*( p)) <= 111 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 79 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3663: goto st40;
		case 3695: goto st40;
	}
	goto tr0;
st40:
	if ( ++( p) == ( pe) )
		goto _test_eof40;
case 40:
	_widec = (*( p));
	if ( (*( p)) > 73 ) {
		if ( 105 <= (*( p)) && (*( p)) <= 105 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 73 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3657: goto st41;
		case 3689: goto st41;
	}
	goto tr0;
st41:
	if ( ++( p) == ( pe) )
		goto _test_eof41;
case 41:
	_widec = (*( p));
	if ( (*( p)) > 76 ) {
		if ( 108 <= (*( p)) && (*( p)) <= 108 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 76 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3660: goto st42;
		case 3692: goto st42;
	}
	goto tr0;
st42:
	if ( ++( p) == ( pe) )
		goto _test_eof42;
case 42:
	_widec = (*( p));
	if ( (*( p)) > 69 ) {
		if ( 101 <= (*( p)) && (*( p)) <= 101 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 69 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3653: goto st43;
		case 3685: goto st43;
	}
	goto tr0;
st43:
	if ( ++( p) == ( pe) )
		goto _test_eof43;
case 43:
	_widec = (*( p));
	if ( (*( p)) > 82 ) {
		if ( 114 <= (*( p)) && (*( p)) <= 114 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 82 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3666: goto st44;
		case 3698: goto st44;
	}
	goto tr0;
st44:
	if ( ++( p) == ( pe) )
		goto _test_eof44;
case 44:
	_widec = (*( p));
	if ( (*( p)) < 83 ) {
		if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) > 83 ) {
		if ( 115 <= (*( p)) && (*( p)) <= 115 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3646: goto st1636;
		case 3667: goto st45;
		case 3699: goto st45;
	}
	goto tr0;
st1636:
	if ( ++( p) == ( pe) )
		goto _test_eof1636;
case 1636:
	switch( (*( p)) ) {
		case 9: goto st1636;
		case 32: goto st1636;
	}
	goto tr2029;
st45:
	if ( ++( p) == ( pe) )
		goto _test_eof45;
case 45:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	if ( _widec == 3646 )
		goto st1636;
	goto tr0;
st46:
	if ( ++( p) == ( pe) )
		goto _test_eof46;
case 46:
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3375: goto st29;
		case 3631: goto st47;
	}
	goto tr0;
st47:
	if ( ++( p) == ( pe) )
		goto _test_eof47;
case 47:
	_widec = (*( p));
	if ( (*( p)) > 83 ) {
		if ( 115 <= (*( p)) && (*( p)) <= 115 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 83 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 69: goto st30;
		case 81: goto st12;
		case 101: goto st30;
		case 113: goto st12;
		case 3667: goto st48;
		case 3699: goto st48;
	}
	goto tr0;
st48:
	if ( ++( p) == ( pe) )
		goto _test_eof48;
case 48:
	_widec = (*( p));
	if ( (*( p)) > 80 ) {
		if ( 112 <= (*( p)) && (*( p)) <= 112 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 80 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3664: goto st49;
		case 3696: goto st49;
	}
	goto tr0;
st49:
	if ( ++( p) == ( pe) )
		goto _test_eof49;
case 49:
	_widec = (*( p));
	if ( (*( p)) > 79 ) {
		if ( 111 <= (*( p)) && (*( p)) <= 111 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 79 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3663: goto st50;
		case 3695: goto st50;
	}
	goto tr0;
st50:
	if ( ++( p) == ( pe) )
		goto _test_eof50;
case 50:
	_widec = (*( p));
	if ( (*( p)) > 73 ) {
		if ( 105 <= (*( p)) && (*( p)) <= 105 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 73 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3657: goto st51;
		case 3689: goto st51;
	}
	goto tr0;
st51:
	if ( ++( p) == ( pe) )
		goto _test_eof51;
case 51:
	_widec = (*( p));
	if ( (*( p)) > 76 ) {
		if ( 108 <= (*( p)) && (*( p)) <= 108 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 76 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3660: goto st52;
		case 3692: goto st52;
	}
	goto tr0;
st52:
	if ( ++( p) == ( pe) )
		goto _test_eof52;
case 52:
	_widec = (*( p));
	if ( (*( p)) > 69 ) {
		if ( 101 <= (*( p)) && (*( p)) <= 101 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 69 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3653: goto st53;
		case 3685: goto st53;
	}
	goto tr0;
st53:
	if ( ++( p) == ( pe) )
		goto _test_eof53;
case 53:
	_widec = (*( p));
	if ( (*( p)) > 82 ) {
		if ( 114 <= (*( p)) && (*( p)) <= 114 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 82 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3666: goto st54;
		case 3698: goto st54;
	}
	goto tr0;
st54:
	if ( ++( p) == ( pe) )
		goto _test_eof54;
case 54:
	_widec = (*( p));
	if ( (*( p)) < 93 ) {
		if ( 83 <= (*( p)) && (*( p)) <= 83 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) > 93 ) {
		if ( 115 <= (*( p)) && (*( p)) <= 115 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3667: goto st55;
		case 3677: goto st1636;
		case 3699: goto st55;
	}
	goto tr0;
st55:
	if ( ++( p) == ( pe) )
		goto _test_eof55;
case 55:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	if ( _widec == 3677 )
		goto st1636;
	goto tr0;
st56:
	if ( ++( p) == ( pe) )
		goto _test_eof56;
case 56:
	switch( (*( p)) ) {
		case 47: goto st6;
		case 72: goto st57;
		case 84: goto st60;
		case 104: goto st57;
		case 116: goto st60;
	}
	goto tr3;
st57:
	if ( ++( p) == ( pe) )
		goto _test_eof57;
case 57:
	switch( (*( p)) ) {
		case 82: goto st58;
		case 114: goto st58;
	}
	goto tr3;
st58:
	if ( ++( p) == ( pe) )
		goto _test_eof58;
case 58:
	if ( (*( p)) == 62 )
		goto st59;
	goto tr3;
st59:
	if ( ++( p) == ( pe) )
		goto _test_eof59;
case 59:
	switch( (*( p)) ) {
		case 0: goto st1637;
		case 9: goto st59;
		case 10: goto st1637;
		case 32: goto st59;
	}
	goto tr3;
st1637:
	if ( ++( p) == ( pe) )
		goto _test_eof1637;
case 1637:
	switch( (*( p)) ) {
		case 0: goto st1637;
		case 10: goto st1637;
	}
	goto tr2030;
st60:
	if ( ++( p) == ( pe) )
		goto _test_eof60;
case 60:
	switch( (*( p)) ) {
		case 65: goto st61;
		case 97: goto st61;
	}
	goto tr3;
st61:
	if ( ++( p) == ( pe) )
		goto _test_eof61;
case 61:
	switch( (*( p)) ) {
		case 66: goto st62;
		case 98: goto st62;
	}
	goto tr3;
st62:
	if ( ++( p) == ( pe) )
		goto _test_eof62;
case 62:
	switch( (*( p)) ) {
		case 76: goto st63;
		case 108: goto st63;
	}
	goto tr3;
st63:
	if ( ++( p) == ( pe) )
		goto _test_eof63;
case 63:
	switch( (*( p)) ) {
		case 69: goto st64;
		case 101: goto st64;
	}
	goto tr3;
st64:
	if ( ++( p) == ( pe) )
		goto _test_eof64;
case 64:
	if ( (*( p)) == 62 )
		goto tr77;
	goto tr3;
st65:
	if ( ++( p) == ( pe) )
		goto _test_eof65;
case 65:
	switch( (*( p)) ) {
		case 47: goto st29;
		case 72: goto st66;
		case 84: goto st68;
		case 104: goto st66;
		case 116: goto st68;
	}
	goto tr3;
st66:
	if ( ++( p) == ( pe) )
		goto _test_eof66;
case 66:
	switch( (*( p)) ) {
		case 82: goto st67;
		case 114: goto st67;
	}
	goto tr3;
st67:
	if ( ++( p) == ( pe) )
		goto _test_eof67;
case 67:
	if ( (*( p)) == 93 )
		goto st59;
	goto tr3;
st68:
	if ( ++( p) == ( pe) )
		goto _test_eof68;
case 68:
	switch( (*( p)) ) {
		case 65: goto st69;
		case 97: goto st69;
	}
	goto tr3;
st69:
	if ( ++( p) == ( pe) )
		goto _test_eof69;
case 69:
	switch( (*( p)) ) {
		case 66: goto st70;
		case 98: goto st70;
	}
	goto tr3;
st70:
	if ( ++( p) == ( pe) )
		goto _test_eof70;
case 70:
	switch( (*( p)) ) {
		case 76: goto st71;
		case 108: goto st71;
	}
	goto tr3;
st71:
	if ( ++( p) == ( pe) )
		goto _test_eof71;
case 71:
	switch( (*( p)) ) {
		case 69: goto st72;
		case 101: goto st72;
	}
	goto tr3;
st72:
	if ( ++( p) == ( pe) )
		goto _test_eof72;
case 72:
	if ( (*( p)) == 93 )
		goto tr77;
	goto tr3;
st73:
	if ( ++( p) == ( pe) )
		goto _test_eof73;
case 73:
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 72: goto st57;
		case 84: goto st60;
		case 104: goto st57;
		case 116: goto st60;
		case 3375: goto st6;
		case 3631: goto st37;
	}
	goto tr3;
st74:
	if ( ++( p) == ( pe) )
		goto _test_eof74;
case 74:
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 72: goto st66;
		case 84: goto st68;
		case 104: goto st66;
		case 116: goto st68;
		case 3375: goto st29;
		case 3631: goto st47;
	}
	goto tr3;
tr2017:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 776 "ext/dtext/dtext.cpp.rl"
	{( act) = 137;}
	goto st1638;
st1638:
	if ( ++( p) == ( pe) )
		goto _test_eof1638;
case 1638:
#line 4359 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) > 60 ) {
		if ( 91 <= (*( p)) && (*( p)) <= 91 ) {
			_widec = (short)(3200 + ((*( p)) - -128));
			if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
		}
	} else if ( (*( p)) >= 60 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 32: goto st4;
		case 3388: goto st5;
		case 3419: goto st28;
		case 3644: goto st36;
		case 3675: goto st46;
	}
	if ( 9 <= _widec && _widec <= 13 )
		goto st4;
	goto tr2026;
tr2018:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1639;
st1639:
	if ( ++( p) == ( pe) )
		goto _test_eof1639;
case 1639:
#line 4394 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr87;
		case 32: goto tr87;
		case 42: goto st76;
	}
	goto tr2026;
tr87:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
	goto st75;
st75:
	if ( ++( p) == ( pe) )
		goto _test_eof75;
case 75:
#line 4409 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr86;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr86;
	}
	goto tr85;
tr85:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1640;
st1640:
	if ( ++( p) == ( pe) )
		goto _test_eof1640;
case 1640:
#line 4426 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2031;
		case 10: goto tr2031;
		case 13: goto tr2031;
	}
	goto st1640;
tr86:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1641;
st1641:
	if ( ++( p) == ( pe) )
		goto _test_eof1641;
case 1641:
#line 4441 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2031;
		case 9: goto tr86;
		case 10: goto tr2031;
		case 13: goto tr2031;
		case 32: goto tr86;
	}
	goto tr85;
st76:
	if ( ++( p) == ( pe) )
		goto _test_eof76;
case 76:
	switch( (*( p)) ) {
		case 9: goto tr87;
		case 32: goto tr87;
		case 42: goto st76;
	}
	goto tr3;
st0:
( cs) = 0;
	goto _out;
tr2020:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1642;
st1642:
	if ( ++( p) == ( pe) )
		goto _test_eof1642;
case 1642:
#line 4471 "ext/dtext/dtext.cpp"
	if ( 49 <= (*( p)) && (*( p)) <= 54 )
		goto tr2033;
	goto tr2026;
tr2033:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st77;
st77:
	if ( ++( p) == ( pe) )
		goto _test_eof77;
case 77:
#line 4483 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr89;
		case 46: goto tr90;
	}
	goto tr3;
tr89:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st78;
st78:
	if ( ++( p) == ( pe) )
		goto _test_eof78;
case 78:
#line 4497 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto tr91;
		case 35: goto tr91;
		case 38: goto tr91;
		case 45: goto tr91;
		case 95: goto tr91;
	}
	if ( (*( p)) < 65 ) {
		if ( 47 <= (*( p)) && (*( p)) <= 58 )
			goto tr91;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr91;
	} else
		goto tr91;
	goto tr3;
tr91:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st79;
st79:
	if ( ++( p) == ( pe) )
		goto _test_eof79;
case 79:
#line 4522 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto st79;
		case 35: goto st79;
		case 38: goto st79;
		case 46: goto tr93;
		case 95: goto st79;
	}
	if ( (*( p)) < 65 ) {
		if ( 45 <= (*( p)) && (*( p)) <= 58 )
			goto st79;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st79;
	} else
		goto st79;
	goto tr3;
tr90:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1643;
tr93:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1643;
st1643:
	if ( ++( p) == ( pe) )
		goto _test_eof1643;
case 1643:
#line 4555 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1643;
		case 32: goto st1643;
	}
	goto tr2034;
tr2021:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1644;
st1644:
	if ( ++( p) == ( pe) )
		goto _test_eof1644;
case 1644:
#line 4569 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 96 )
		goto st80;
	goto tr2026;
st80:
	if ( ++( p) == ( pe) )
		goto _test_eof80;
case 80:
	if ( (*( p)) == 96 )
		goto st81;
	goto tr3;
tr96:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st81;
st81:
	if ( ++( p) == ( pe) )
		goto _test_eof81;
case 81:
#line 4590 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr95;
		case 9: goto tr96;
		case 10: goto tr95;
		case 32: goto tr96;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr97;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr97;
	} else
		goto tr97;
	goto tr3;
tr106:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st82;
tr95:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st82;
st82:
	if ( ++( p) == ( pe) )
		goto _test_eof82;
case 82:
#line 4620 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr99;
		case 10: goto tr99;
	}
	goto tr98;
tr98:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st83;
st83:
	if ( ++( p) == ( pe) )
		goto _test_eof83;
case 83:
#line 4634 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr101;
		case 10: goto tr101;
	}
	goto st83;
tr101:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st84;
tr99:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st84;
st84:
	if ( ++( p) == ( pe) )
		goto _test_eof84;
case 84:
#line 4654 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr101;
		case 10: goto tr101;
		case 96: goto st85;
	}
	goto st83;
st85:
	if ( ++( p) == ( pe) )
		goto _test_eof85;
case 85:
	switch( (*( p)) ) {
		case 0: goto tr101;
		case 10: goto tr101;
		case 96: goto st86;
	}
	goto st83;
st86:
	if ( ++( p) == ( pe) )
		goto _test_eof86;
case 86:
	switch( (*( p)) ) {
		case 0: goto tr101;
		case 10: goto tr101;
		case 96: goto st87;
	}
	goto st83;
st87:
	if ( ++( p) == ( pe) )
		goto _test_eof87;
case 87:
	switch( (*( p)) ) {
		case 0: goto tr105;
		case 9: goto st87;
		case 10: goto tr105;
		case 32: goto st87;
	}
	goto st83;
tr97:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st88;
st88:
	if ( ++( p) == ( pe) )
		goto _test_eof88;
case 88:
#line 4700 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr106;
		case 9: goto tr107;
		case 10: goto tr106;
		case 32: goto tr107;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st88;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st88;
	} else
		goto st88;
	goto tr3;
tr107:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st89;
st89:
	if ( ++( p) == ( pe) )
		goto _test_eof89;
case 89:
#line 4724 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st82;
		case 9: goto st89;
		case 10: goto st82;
		case 32: goto st89;
	}
	goto tr3;
tr2022:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 776 "ext/dtext/dtext.cpp.rl"
	{( act) = 137;}
	goto st1645;
st1645:
	if ( ++( p) == ( pe) )
		goto _test_eof1645;
case 1645:
#line 4742 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st6;
		case 66: goto st90;
		case 67: goto st100;
		case 69: goto st109;
		case 72: goto st57;
		case 78: goto st118;
		case 81: goto st95;
		case 83: goto st126;
		case 84: goto st134;
		case 98: goto st90;
		case 99: goto st100;
		case 101: goto st109;
		case 104: goto st57;
		case 110: goto st118;
		case 113: goto st95;
		case 115: goto st126;
		case 116: goto st134;
	}
	goto tr2026;
st90:
	if ( ++( p) == ( pe) )
		goto _test_eof90;
case 90:
	switch( (*( p)) ) {
		case 76: goto st91;
		case 108: goto st91;
	}
	goto tr3;
st91:
	if ( ++( p) == ( pe) )
		goto _test_eof91;
case 91:
	switch( (*( p)) ) {
		case 79: goto st92;
		case 111: goto st92;
	}
	goto tr3;
st92:
	if ( ++( p) == ( pe) )
		goto _test_eof92;
case 92:
	switch( (*( p)) ) {
		case 67: goto st93;
		case 99: goto st93;
	}
	goto tr3;
st93:
	if ( ++( p) == ( pe) )
		goto _test_eof93;
case 93:
	switch( (*( p)) ) {
		case 75: goto st94;
		case 107: goto st94;
	}
	goto tr3;
st94:
	if ( ++( p) == ( pe) )
		goto _test_eof94;
case 94:
	switch( (*( p)) ) {
		case 81: goto st95;
		case 113: goto st95;
	}
	goto tr3;
st95:
	if ( ++( p) == ( pe) )
		goto _test_eof95;
case 95:
	switch( (*( p)) ) {
		case 85: goto st96;
		case 117: goto st96;
	}
	goto tr3;
st96:
	if ( ++( p) == ( pe) )
		goto _test_eof96;
case 96:
	switch( (*( p)) ) {
		case 79: goto st97;
		case 111: goto st97;
	}
	goto tr3;
st97:
	if ( ++( p) == ( pe) )
		goto _test_eof97;
case 97:
	switch( (*( p)) ) {
		case 84: goto st98;
		case 116: goto st98;
	}
	goto tr3;
st98:
	if ( ++( p) == ( pe) )
		goto _test_eof98;
case 98:
	switch( (*( p)) ) {
		case 69: goto st99;
		case 101: goto st99;
	}
	goto tr3;
st99:
	if ( ++( p) == ( pe) )
		goto _test_eof99;
case 99:
	if ( (*( p)) == 62 )
		goto st1646;
	goto tr3;
st1646:
	if ( ++( p) == ( pe) )
		goto _test_eof1646;
case 1646:
	if ( (*( p)) == 32 )
		goto st1646;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1646;
	goto tr2043;
st100:
	if ( ++( p) == ( pe) )
		goto _test_eof100;
case 100:
	switch( (*( p)) ) {
		case 79: goto st101;
		case 111: goto st101;
	}
	goto tr3;
st101:
	if ( ++( p) == ( pe) )
		goto _test_eof101;
case 101:
	switch( (*( p)) ) {
		case 68: goto st102;
		case 100: goto st102;
	}
	goto tr3;
st102:
	if ( ++( p) == ( pe) )
		goto _test_eof102;
case 102:
	switch( (*( p)) ) {
		case 69: goto st103;
		case 101: goto st103;
	}
	goto tr3;
st103:
	if ( ++( p) == ( pe) )
		goto _test_eof103;
case 103:
	switch( (*( p)) ) {
		case 9: goto st104;
		case 32: goto st104;
		case 61: goto st105;
		case 62: goto tr126;
	}
	goto tr3;
st104:
	if ( ++( p) == ( pe) )
		goto _test_eof104;
case 104:
	switch( (*( p)) ) {
		case 9: goto st104;
		case 32: goto st104;
		case 61: goto st105;
	}
	goto tr3;
st105:
	if ( ++( p) == ( pe) )
		goto _test_eof105;
case 105:
	switch( (*( p)) ) {
		case 9: goto st105;
		case 32: goto st105;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr127;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr127;
	} else
		goto tr127;
	goto tr3;
tr127:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st106;
st106:
	if ( ++( p) == ( pe) )
		goto _test_eof106;
case 106:
#line 4933 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 62 )
		goto tr129;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st106;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st106;
	} else
		goto st106;
	goto tr3;
tr129:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1647;
st1647:
	if ( ++( p) == ( pe) )
		goto _test_eof1647;
case 1647:
#line 4955 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr131;
		case 9: goto st107;
		case 10: goto tr131;
		case 32: goto st107;
	}
	goto tr2044;
st107:
	if ( ++( p) == ( pe) )
		goto _test_eof107;
case 107:
	switch( (*( p)) ) {
		case 0: goto tr131;
		case 9: goto st107;
		case 10: goto tr131;
		case 32: goto st107;
	}
	goto tr130;
tr126:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1648;
st1648:
	if ( ++( p) == ( pe) )
		goto _test_eof1648;
case 1648:
#line 4982 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr134;
		case 9: goto st108;
		case 10: goto tr134;
		case 32: goto st108;
	}
	goto tr2045;
st108:
	if ( ++( p) == ( pe) )
		goto _test_eof108;
case 108:
	switch( (*( p)) ) {
		case 0: goto tr134;
		case 9: goto st108;
		case 10: goto tr134;
		case 32: goto st108;
	}
	goto tr133;
st109:
	if ( ++( p) == ( pe) )
		goto _test_eof109;
case 109:
	switch( (*( p)) ) {
		case 88: goto st110;
		case 120: goto st110;
	}
	goto tr3;
st110:
	if ( ++( p) == ( pe) )
		goto _test_eof110;
case 110:
	switch( (*( p)) ) {
		case 80: goto st111;
		case 112: goto st111;
	}
	goto tr3;
st111:
	if ( ++( p) == ( pe) )
		goto _test_eof111;
case 111:
	switch( (*( p)) ) {
		case 65: goto st112;
		case 97: goto st112;
	}
	goto tr3;
st112:
	if ( ++( p) == ( pe) )
		goto _test_eof112;
case 112:
	switch( (*( p)) ) {
		case 78: goto st113;
		case 110: goto st113;
	}
	goto tr3;
st113:
	if ( ++( p) == ( pe) )
		goto _test_eof113;
case 113:
	switch( (*( p)) ) {
		case 68: goto st114;
		case 100: goto st114;
	}
	goto tr3;
st114:
	if ( ++( p) == ( pe) )
		goto _test_eof114;
case 114:
	switch( (*( p)) ) {
		case 9: goto st115;
		case 32: goto st115;
		case 61: goto st117;
		case 62: goto st1650;
	}
	goto tr3;
tr145:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st115;
st115:
	if ( ++( p) == ( pe) )
		goto _test_eof115;
case 115:
#line 5065 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr145;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr145;
		case 61: goto tr146;
		case 62: goto tr147;
	}
	goto tr144;
tr144:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st116;
st116:
	if ( ++( p) == ( pe) )
		goto _test_eof116;
case 116:
#line 5084 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 10: goto tr3;
		case 13: goto tr3;
		case 62: goto tr149;
	}
	goto st116;
tr149:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1649;
tr147:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1649;
st1649:
	if ( ++( p) == ( pe) )
		goto _test_eof1649;
case 1649:
#line 5106 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 32 )
		goto st1649;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1649;
	goto tr2046;
tr146:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st117;
st117:
	if ( ++( p) == ( pe) )
		goto _test_eof117;
case 117:
#line 5120 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr146;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr146;
		case 62: goto tr147;
	}
	goto tr144;
st1650:
	if ( ++( p) == ( pe) )
		goto _test_eof1650;
case 1650:
	if ( (*( p)) == 32 )
		goto st1650;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1650;
	goto tr2048;
st118:
	if ( ++( p) == ( pe) )
		goto _test_eof118;
case 118:
	switch( (*( p)) ) {
		case 79: goto st119;
		case 111: goto st119;
	}
	goto tr3;
st119:
	if ( ++( p) == ( pe) )
		goto _test_eof119;
case 119:
	switch( (*( p)) ) {
		case 68: goto st120;
		case 100: goto st120;
	}
	goto tr3;
st120:
	if ( ++( p) == ( pe) )
		goto _test_eof120;
case 120:
	switch( (*( p)) ) {
		case 84: goto st121;
		case 116: goto st121;
	}
	goto tr3;
st121:
	if ( ++( p) == ( pe) )
		goto _test_eof121;
case 121:
	switch( (*( p)) ) {
		case 69: goto st122;
		case 101: goto st122;
	}
	goto tr3;
st122:
	if ( ++( p) == ( pe) )
		goto _test_eof122;
case 122:
	switch( (*( p)) ) {
		case 88: goto st123;
		case 120: goto st123;
	}
	goto tr3;
st123:
	if ( ++( p) == ( pe) )
		goto _test_eof123;
case 123:
	switch( (*( p)) ) {
		case 84: goto st124;
		case 116: goto st124;
	}
	goto tr3;
st124:
	if ( ++( p) == ( pe) )
		goto _test_eof124;
case 124:
	if ( (*( p)) == 62 )
		goto tr156;
	goto tr3;
tr156:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1651;
st1651:
	if ( ++( p) == ( pe) )
		goto _test_eof1651;
case 1651:
#line 5208 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr158;
		case 9: goto st125;
		case 10: goto tr158;
		case 32: goto st125;
	}
	goto tr2049;
st125:
	if ( ++( p) == ( pe) )
		goto _test_eof125;
case 125:
	switch( (*( p)) ) {
		case 0: goto tr158;
		case 9: goto st125;
		case 10: goto tr158;
		case 32: goto st125;
	}
	goto tr157;
st126:
	if ( ++( p) == ( pe) )
		goto _test_eof126;
case 126:
	switch( (*( p)) ) {
		case 80: goto st127;
		case 112: goto st127;
	}
	goto tr3;
st127:
	if ( ++( p) == ( pe) )
		goto _test_eof127;
case 127:
	switch( (*( p)) ) {
		case 79: goto st128;
		case 111: goto st128;
	}
	goto tr3;
st128:
	if ( ++( p) == ( pe) )
		goto _test_eof128;
case 128:
	switch( (*( p)) ) {
		case 73: goto st129;
		case 105: goto st129;
	}
	goto tr3;
st129:
	if ( ++( p) == ( pe) )
		goto _test_eof129;
case 129:
	switch( (*( p)) ) {
		case 76: goto st130;
		case 108: goto st130;
	}
	goto tr3;
st130:
	if ( ++( p) == ( pe) )
		goto _test_eof130;
case 130:
	switch( (*( p)) ) {
		case 69: goto st131;
		case 101: goto st131;
	}
	goto tr3;
st131:
	if ( ++( p) == ( pe) )
		goto _test_eof131;
case 131:
	switch( (*( p)) ) {
		case 82: goto st132;
		case 114: goto st132;
	}
	goto tr3;
st132:
	if ( ++( p) == ( pe) )
		goto _test_eof132;
case 132:
	switch( (*( p)) ) {
		case 62: goto st1652;
		case 83: goto st133;
		case 115: goto st133;
	}
	goto tr3;
st1652:
	if ( ++( p) == ( pe) )
		goto _test_eof1652;
case 1652:
	if ( (*( p)) == 32 )
		goto st1652;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1652;
	goto tr2050;
st133:
	if ( ++( p) == ( pe) )
		goto _test_eof133;
case 133:
	if ( (*( p)) == 62 )
		goto st1652;
	goto tr3;
st134:
	if ( ++( p) == ( pe) )
		goto _test_eof134;
case 134:
	switch( (*( p)) ) {
		case 65: goto st61;
		case 78: goto st135;
		case 97: goto st61;
		case 110: goto st135;
	}
	goto tr3;
st135:
	if ( ++( p) == ( pe) )
		goto _test_eof135;
case 135:
	if ( (*( p)) == 62 )
		goto tr169;
	goto tr3;
tr2023:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 776 "ext/dtext/dtext.cpp.rl"
	{( act) = 137;}
	goto st1653;
st1653:
	if ( ++( p) == ( pe) )
		goto _test_eof1653;
case 1653:
#line 5335 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st29;
		case 67: goto st136;
		case 69: goto st143;
		case 72: goto st66;
		case 78: goto st152;
		case 81: goto st159;
		case 83: goto st164;
		case 84: goto st172;
		case 99: goto st136;
		case 101: goto st143;
		case 104: goto st66;
		case 110: goto st152;
		case 113: goto st159;
		case 115: goto st164;
		case 116: goto st172;
	}
	goto tr2026;
st136:
	if ( ++( p) == ( pe) )
		goto _test_eof136;
case 136:
	switch( (*( p)) ) {
		case 79: goto st137;
		case 111: goto st137;
	}
	goto tr3;
st137:
	if ( ++( p) == ( pe) )
		goto _test_eof137;
case 137:
	switch( (*( p)) ) {
		case 68: goto st138;
		case 100: goto st138;
	}
	goto tr3;
st138:
	if ( ++( p) == ( pe) )
		goto _test_eof138;
case 138:
	switch( (*( p)) ) {
		case 69: goto st139;
		case 101: goto st139;
	}
	goto tr3;
st139:
	if ( ++( p) == ( pe) )
		goto _test_eof139;
case 139:
	switch( (*( p)) ) {
		case 9: goto st140;
		case 32: goto st140;
		case 61: goto st141;
		case 93: goto tr126;
	}
	goto tr3;
st140:
	if ( ++( p) == ( pe) )
		goto _test_eof140;
case 140:
	switch( (*( p)) ) {
		case 9: goto st140;
		case 32: goto st140;
		case 61: goto st141;
	}
	goto tr3;
st141:
	if ( ++( p) == ( pe) )
		goto _test_eof141;
case 141:
	switch( (*( p)) ) {
		case 9: goto st141;
		case 32: goto st141;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr175;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr175;
	} else
		goto tr175;
	goto tr3;
tr175:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st142;
st142:
	if ( ++( p) == ( pe) )
		goto _test_eof142;
case 142:
#line 5427 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto tr129;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st142;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st142;
	} else
		goto st142;
	goto tr3;
st143:
	if ( ++( p) == ( pe) )
		goto _test_eof143;
case 143:
	switch( (*( p)) ) {
		case 88: goto st144;
		case 120: goto st144;
	}
	goto tr3;
st144:
	if ( ++( p) == ( pe) )
		goto _test_eof144;
case 144:
	switch( (*( p)) ) {
		case 80: goto st145;
		case 112: goto st145;
	}
	goto tr3;
st145:
	if ( ++( p) == ( pe) )
		goto _test_eof145;
case 145:
	switch( (*( p)) ) {
		case 65: goto st146;
		case 97: goto st146;
	}
	goto tr3;
st146:
	if ( ++( p) == ( pe) )
		goto _test_eof146;
case 146:
	switch( (*( p)) ) {
		case 78: goto st147;
		case 110: goto st147;
	}
	goto tr3;
st147:
	if ( ++( p) == ( pe) )
		goto _test_eof147;
case 147:
	switch( (*( p)) ) {
		case 68: goto st148;
		case 100: goto st148;
	}
	goto tr3;
st148:
	if ( ++( p) == ( pe) )
		goto _test_eof148;
case 148:
	switch( (*( p)) ) {
		case 9: goto st149;
		case 32: goto st149;
		case 61: goto st151;
		case 93: goto st1650;
	}
	goto tr3;
tr185:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st149;
st149:
	if ( ++( p) == ( pe) )
		goto _test_eof149;
case 149:
#line 5503 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr185;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr185;
		case 61: goto tr186;
		case 93: goto tr147;
	}
	goto tr184;
tr184:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st150;
st150:
	if ( ++( p) == ( pe) )
		goto _test_eof150;
case 150:
#line 5522 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 10: goto tr3;
		case 13: goto tr3;
		case 93: goto tr149;
	}
	goto st150;
tr186:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st151;
st151:
	if ( ++( p) == ( pe) )
		goto _test_eof151;
case 151:
#line 5538 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr186;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr186;
		case 93: goto tr147;
	}
	goto tr184;
st152:
	if ( ++( p) == ( pe) )
		goto _test_eof152;
case 152:
	switch( (*( p)) ) {
		case 79: goto st153;
		case 111: goto st153;
	}
	goto tr3;
st153:
	if ( ++( p) == ( pe) )
		goto _test_eof153;
case 153:
	switch( (*( p)) ) {
		case 68: goto st154;
		case 100: goto st154;
	}
	goto tr3;
st154:
	if ( ++( p) == ( pe) )
		goto _test_eof154;
case 154:
	switch( (*( p)) ) {
		case 84: goto st155;
		case 116: goto st155;
	}
	goto tr3;
st155:
	if ( ++( p) == ( pe) )
		goto _test_eof155;
case 155:
	switch( (*( p)) ) {
		case 69: goto st156;
		case 101: goto st156;
	}
	goto tr3;
st156:
	if ( ++( p) == ( pe) )
		goto _test_eof156;
case 156:
	switch( (*( p)) ) {
		case 88: goto st157;
		case 120: goto st157;
	}
	goto tr3;
st157:
	if ( ++( p) == ( pe) )
		goto _test_eof157;
case 157:
	switch( (*( p)) ) {
		case 84: goto st158;
		case 116: goto st158;
	}
	goto tr3;
st158:
	if ( ++( p) == ( pe) )
		goto _test_eof158;
case 158:
	if ( (*( p)) == 93 )
		goto tr156;
	goto tr3;
st159:
	if ( ++( p) == ( pe) )
		goto _test_eof159;
case 159:
	switch( (*( p)) ) {
		case 85: goto st160;
		case 117: goto st160;
	}
	goto tr3;
st160:
	if ( ++( p) == ( pe) )
		goto _test_eof160;
case 160:
	switch( (*( p)) ) {
		case 79: goto st161;
		case 111: goto st161;
	}
	goto tr3;
st161:
	if ( ++( p) == ( pe) )
		goto _test_eof161;
case 161:
	switch( (*( p)) ) {
		case 84: goto st162;
		case 116: goto st162;
	}
	goto tr3;
st162:
	if ( ++( p) == ( pe) )
		goto _test_eof162;
case 162:
	switch( (*( p)) ) {
		case 69: goto st163;
		case 101: goto st163;
	}
	goto tr3;
st163:
	if ( ++( p) == ( pe) )
		goto _test_eof163;
case 163:
	if ( (*( p)) == 93 )
		goto st1646;
	goto tr3;
st164:
	if ( ++( p) == ( pe) )
		goto _test_eof164;
case 164:
	switch( (*( p)) ) {
		case 80: goto st165;
		case 112: goto st165;
	}
	goto tr3;
st165:
	if ( ++( p) == ( pe) )
		goto _test_eof165;
case 165:
	switch( (*( p)) ) {
		case 79: goto st166;
		case 111: goto st166;
	}
	goto tr3;
st166:
	if ( ++( p) == ( pe) )
		goto _test_eof166;
case 166:
	switch( (*( p)) ) {
		case 73: goto st167;
		case 105: goto st167;
	}
	goto tr3;
st167:
	if ( ++( p) == ( pe) )
		goto _test_eof167;
case 167:
	switch( (*( p)) ) {
		case 76: goto st168;
		case 108: goto st168;
	}
	goto tr3;
st168:
	if ( ++( p) == ( pe) )
		goto _test_eof168;
case 168:
	switch( (*( p)) ) {
		case 69: goto st169;
		case 101: goto st169;
	}
	goto tr3;
st169:
	if ( ++( p) == ( pe) )
		goto _test_eof169;
case 169:
	switch( (*( p)) ) {
		case 82: goto st170;
		case 114: goto st170;
	}
	goto tr3;
st170:
	if ( ++( p) == ( pe) )
		goto _test_eof170;
case 170:
	switch( (*( p)) ) {
		case 83: goto st171;
		case 93: goto st1652;
		case 115: goto st171;
	}
	goto tr3;
st171:
	if ( ++( p) == ( pe) )
		goto _test_eof171;
case 171:
	if ( (*( p)) == 93 )
		goto st1652;
	goto tr3;
st172:
	if ( ++( p) == ( pe) )
		goto _test_eof172;
case 172:
	switch( (*( p)) ) {
		case 65: goto st69;
		case 78: goto st173;
		case 97: goto st69;
		case 110: goto st173;
	}
	goto tr3;
st173:
	if ( ++( p) == ( pe) )
		goto _test_eof173;
case 173:
	if ( (*( p)) == 93 )
		goto tr169;
	goto tr3;
tr2024:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 776 "ext/dtext/dtext.cpp.rl"
	{( act) = 137;}
	goto st1654;
st1654:
	if ( ++( p) == ( pe) )
		goto _test_eof1654;
case 1654:
#line 5751 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 66: goto st90;
		case 67: goto st100;
		case 69: goto st109;
		case 72: goto st57;
		case 78: goto st118;
		case 81: goto st95;
		case 83: goto st126;
		case 84: goto st134;
		case 98: goto st90;
		case 99: goto st100;
		case 101: goto st109;
		case 104: goto st57;
		case 110: goto st118;
		case 113: goto st95;
		case 115: goto st126;
		case 116: goto st134;
		case 3375: goto st6;
		case 3631: goto st37;
	}
	goto tr2026;
tr2025:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 776 "ext/dtext/dtext.cpp.rl"
	{( act) = 137;}
	goto st1655;
st1655:
	if ( ++( p) == ( pe) )
		goto _test_eof1655;
case 1655:
#line 5790 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 67: goto st136;
		case 69: goto st143;
		case 72: goto st66;
		case 78: goto st152;
		case 81: goto st159;
		case 83: goto st164;
		case 84: goto st172;
		case 99: goto st136;
		case 101: goto st143;
		case 104: goto st66;
		case 110: goto st152;
		case 113: goto st159;
		case 115: goto st164;
		case 116: goto st172;
		case 3375: goto st29;
		case 3631: goto st47;
	}
	goto tr2026;
tr206:
#line 302 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_html_escaped((*( p))); }}
	goto st1656;
tr212:
#line 294 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_B, { ts, te }); }}
	goto st1656;
tr213:
#line 296 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_I, { ts, te }); }}
	goto st1656;
tr214:
#line 298 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_S, { ts, te }); }}
	goto st1656;
tr219:
#line 300 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_U, { ts, te }); }}
	goto st1656;
tr220:
#line 293 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_B, "<strong>"); }}
	goto st1656;
tr222:
#line 295 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_I, "<em>"); }}
	goto st1656;
tr223:
#line 297 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_S, "<s>"); }}
	goto st1656;
tr229:
#line 299 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_U, "<u>"); }}
	goto st1656;
tr2057:
#line 302 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append_html_escaped((*( p))); }}
	goto st1656;
tr2058:
#line 301 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1656;
tr2061:
#line 302 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_html_escaped((*( p))); }}
	goto st1656;
st1656:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1656;
case 1656:
#line 1 "NONE"
	{( ts) = ( p);}
#line 5873 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2058;
		case 60: goto tr2059;
		case 91: goto tr2060;
	}
	goto tr2057;
tr2059:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1657;
st1657:
	if ( ++( p) == ( pe) )
		goto _test_eof1657;
case 1657:
#line 5888 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st174;
		case 66: goto st184;
		case 69: goto st185;
		case 73: goto st186;
		case 83: goto st187;
		case 85: goto st192;
		case 98: goto st184;
		case 101: goto st185;
		case 105: goto st186;
		case 115: goto st187;
		case 117: goto st192;
	}
	goto tr2061;
st174:
	if ( ++( p) == ( pe) )
		goto _test_eof174;
case 174:
	switch( (*( p)) ) {
		case 66: goto st175;
		case 69: goto st176;
		case 73: goto st177;
		case 83: goto st178;
		case 85: goto st183;
		case 98: goto st175;
		case 101: goto st176;
		case 105: goto st177;
		case 115: goto st178;
		case 117: goto st183;
	}
	goto tr206;
st175:
	if ( ++( p) == ( pe) )
		goto _test_eof175;
case 175:
	if ( (*( p)) == 62 )
		goto tr212;
	goto tr206;
st176:
	if ( ++( p) == ( pe) )
		goto _test_eof176;
case 176:
	switch( (*( p)) ) {
		case 77: goto st177;
		case 109: goto st177;
	}
	goto tr206;
st177:
	if ( ++( p) == ( pe) )
		goto _test_eof177;
case 177:
	if ( (*( p)) == 62 )
		goto tr213;
	goto tr206;
st178:
	if ( ++( p) == ( pe) )
		goto _test_eof178;
case 178:
	switch( (*( p)) ) {
		case 62: goto tr214;
		case 84: goto st179;
		case 116: goto st179;
	}
	goto tr206;
st179:
	if ( ++( p) == ( pe) )
		goto _test_eof179;
case 179:
	switch( (*( p)) ) {
		case 82: goto st180;
		case 114: goto st180;
	}
	goto tr206;
st180:
	if ( ++( p) == ( pe) )
		goto _test_eof180;
case 180:
	switch( (*( p)) ) {
		case 79: goto st181;
		case 111: goto st181;
	}
	goto tr206;
st181:
	if ( ++( p) == ( pe) )
		goto _test_eof181;
case 181:
	switch( (*( p)) ) {
		case 78: goto st182;
		case 110: goto st182;
	}
	goto tr206;
st182:
	if ( ++( p) == ( pe) )
		goto _test_eof182;
case 182:
	switch( (*( p)) ) {
		case 71: goto st175;
		case 103: goto st175;
	}
	goto tr206;
st183:
	if ( ++( p) == ( pe) )
		goto _test_eof183;
case 183:
	if ( (*( p)) == 62 )
		goto tr219;
	goto tr206;
st184:
	if ( ++( p) == ( pe) )
		goto _test_eof184;
case 184:
	if ( (*( p)) == 62 )
		goto tr220;
	goto tr206;
st185:
	if ( ++( p) == ( pe) )
		goto _test_eof185;
case 185:
	switch( (*( p)) ) {
		case 77: goto st186;
		case 109: goto st186;
	}
	goto tr206;
st186:
	if ( ++( p) == ( pe) )
		goto _test_eof186;
case 186:
	if ( (*( p)) == 62 )
		goto tr222;
	goto tr206;
st187:
	if ( ++( p) == ( pe) )
		goto _test_eof187;
case 187:
	switch( (*( p)) ) {
		case 62: goto tr223;
		case 84: goto st188;
		case 116: goto st188;
	}
	goto tr206;
st188:
	if ( ++( p) == ( pe) )
		goto _test_eof188;
case 188:
	switch( (*( p)) ) {
		case 82: goto st189;
		case 114: goto st189;
	}
	goto tr206;
st189:
	if ( ++( p) == ( pe) )
		goto _test_eof189;
case 189:
	switch( (*( p)) ) {
		case 79: goto st190;
		case 111: goto st190;
	}
	goto tr206;
st190:
	if ( ++( p) == ( pe) )
		goto _test_eof190;
case 190:
	switch( (*( p)) ) {
		case 78: goto st191;
		case 110: goto st191;
	}
	goto tr206;
st191:
	if ( ++( p) == ( pe) )
		goto _test_eof191;
case 191:
	switch( (*( p)) ) {
		case 71: goto st184;
		case 103: goto st184;
	}
	goto tr206;
st192:
	if ( ++( p) == ( pe) )
		goto _test_eof192;
case 192:
	if ( (*( p)) == 62 )
		goto tr229;
	goto tr206;
tr2060:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1658;
st1658:
	if ( ++( p) == ( pe) )
		goto _test_eof1658;
case 1658:
#line 6080 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st193;
		case 66: goto st198;
		case 73: goto st199;
		case 83: goto st200;
		case 85: goto st201;
		case 98: goto st198;
		case 105: goto st199;
		case 115: goto st200;
		case 117: goto st201;
	}
	goto tr2061;
st193:
	if ( ++( p) == ( pe) )
		goto _test_eof193;
case 193:
	switch( (*( p)) ) {
		case 66: goto st194;
		case 73: goto st195;
		case 83: goto st196;
		case 85: goto st197;
		case 98: goto st194;
		case 105: goto st195;
		case 115: goto st196;
		case 117: goto st197;
	}
	goto tr206;
st194:
	if ( ++( p) == ( pe) )
		goto _test_eof194;
case 194:
	if ( (*( p)) == 93 )
		goto tr212;
	goto tr206;
st195:
	if ( ++( p) == ( pe) )
		goto _test_eof195;
case 195:
	if ( (*( p)) == 93 )
		goto tr213;
	goto tr206;
st196:
	if ( ++( p) == ( pe) )
		goto _test_eof196;
case 196:
	if ( (*( p)) == 93 )
		goto tr214;
	goto tr206;
st197:
	if ( ++( p) == ( pe) )
		goto _test_eof197;
case 197:
	if ( (*( p)) == 93 )
		goto tr219;
	goto tr206;
st198:
	if ( ++( p) == ( pe) )
		goto _test_eof198;
case 198:
	if ( (*( p)) == 93 )
		goto tr220;
	goto tr206;
st199:
	if ( ++( p) == ( pe) )
		goto _test_eof199;
case 199:
	if ( (*( p)) == 93 )
		goto tr222;
	goto tr206;
st200:
	if ( ++( p) == ( pe) )
		goto _test_eof200;
case 200:
	if ( (*( p)) == 93 )
		goto tr223;
	goto tr206;
st201:
	if ( ++( p) == ( pe) )
		goto _test_eof201;
case 201:
	if ( (*( p)) == 93 )
		goto tr229;
	goto tr206;
tr234:
#line 1 "NONE"
	{	switch( ( act) ) {
	case 46:
	{{( p) = ((( te)))-1;}
    append_bare_named_url({ b1, b2 + 1 }, { a1, a2 });
  }
	break;
	case 47:
	{{( p) = ((( te)))-1;}
    append_named_url({ b1, b2 }, { a1, a2 });
  }
	break;
	case 48:
	{{( p) = ((( te)))-1;}
    append_named_url({ a1, a2 }, { b1, b2 });
  }
	break;
	case 49:
	{{( p) = ((( te)))-1;}
    append_named_url({ g1, g2 }, { f1, f2 });
  }
	break;
	case 50:
	{{( p) = ((( te)))-1;}
    append_bare_unnamed_url({ ts, te });
  }
	break;
	case 52:
	{{( p) = ((( te)))-1;}
    append_mention({ a1, a2 + 1 });
  }
	break;
	case 65:
	{{( p) = ((( te)))-1;}
    if(options.f_allow_color) {
      dstack_open_element(INLINE_COLOR, "<span class=\"dtext-color-");
      append_uri_escaped({ a1, a2 });
      append("\">");
    }
  }
	break;
	case 66:
	{{( p) = ((( te)))-1;}
    if(options.f_allow_color) {
        dstack_open_element(INLINE_COLOR, "<span class=\"dtext-color\" style=\"color: ");
      if(a1[0] == '#') {
        append("#");
        append_uri_escaped({ a1 + 1, a2 });
      } else {
        append_uri_escaped({ a1, a2 });
      }
      append("\">");
    }
  }
	break;
	case 67:
	{{( p) = ((( te)))-1;}
    if(options.f_allow_color) {
      dstack_close_element(INLINE_COLOR, { ts, te });
    }
  }
	break;
	case 69:
	{{( p) = ((( te)))-1;}
    append_inline_code({ a1, a2 });
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1885;}}
  }
	break;
	case 80:
	{{( p) = ((( te)))-1;}
    g_debug("inline newline2");

    if (dstack_check(BLOCK_P)) {
      dstack_rewind();
    } else if (header_mode) {
      dstack_close_leaf_blocks();
    } else {
      dstack_close_list();
    }

    if (options.f_inline) {
      append(" ");
    }

    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }
	break;
	case 81:
	{{( p) = ((( te)))-1;}
    g_debug("inline newline");

    if (header_mode) {
      dstack_close_leaf_blocks();
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    } else if (dstack_is_open(BLOCK_UL)) {
      dstack_close_list();
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    } else {
      append("<br>");
    }
  }
	break;
	case 98:
	{{( p) = ((( te)))-1;}
    append({ ts, te });
  }
	break;
	case 99:
	{{( p) = ((( te)))-1;}
    append_html_escaped((*( p)));
  }
	break;
	default:
	{{( p) = ((( te)))-1;}}
	break;
	}
	}
	goto st1659;
tr237:
#line 592 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append({ ts, te });
  }}
	goto st1659;
tr241:
#line 596 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1659;
tr243:
#line 556 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    g_debug("inline newline");

    if (header_mode) {
      dstack_close_leaf_blocks();
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    } else if (dstack_is_open(BLOCK_UL)) {
      dstack_close_list();
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    } else {
      append("<br>");
    }
  }}
	goto st1659;
tr278:
#line 532 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TD, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1659;
tr279:
#line 526 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TH, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1659;
tr288:
#line 515 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( a1))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr300:
#line 475 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr326:
#line 538 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    g_debug("inline newline2");

    if (dstack_check(BLOCK_P)) {
      dstack_rewind();
    } else if (header_mode) {
      dstack_close_leaf_blocks();
    } else {
      dstack_close_list();
    }

    if (options.f_inline) {
      append(" ");
    }

    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr330:
#line 420 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    g_debug("inline [/tn]");

    if (dstack_check(INLINE_TN)) {
      dstack_close_element(INLINE_TN, { ts, te });
    } else if (dstack_close_element(BLOCK_TN, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1659;
tr351:
#line 485 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_is_open(INLINE_SPOILER)) {
      dstack_close_element(INLINE_SPOILER, { ts, te });
    } else if (dstack_is_open(BLOCK_SPOILER)) {
      dstack_close_until(BLOCK_SPOILER);
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    } else {
      append_html_escaped({ ts, te });
    }
  }}
	goto st1659;
tr358:
#line 504 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr361:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 504 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr372:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 504 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr454:
#line 373 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_bare_named_url({ b1, b2 + 1 }, { a1, a2 });
  }}
	goto st1659;
tr520:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 377 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ b1, b2 }, { a1, a2 });
  }}
	goto st1659;
tr530:
#line 575 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("'"); }}
	goto st1659;
tr535:
#line 571 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&amp;"); }}
	goto st1659;
tr538:
#line 576 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("'"); }}
	goto st1659;
tr540:
#line 579 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('*'); }}
	goto st1659;
tr546:
#line 580 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append(':'); }}
	goto st1659;
tr550:
#line 581 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('@'); }}
	goto st1659;
tr556:
#line 582 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('`'); }}
	goto st1659;
tr557:
#line 573 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&gt;"); }}
	goto st1659;
tr565:
#line 577 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('{'); }}
	goto st1659;
tr566:
#line 578 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('['); }}
	goto st1659;
tr567:
#line 572 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&lt;"); }}
	goto st1659;
tr570:
#line 583 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('#'); }}
	goto st1659;
tr576:
#line 584 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('.'); }}
	goto st1659;
tr580:
#line 574 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&quot;"); }}
	goto st1659;
tr791:
#line 332 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_id_link("dmail", "dmail", "/dmails/", { a1, a2 }); }}
	goto st1659;
tr812:
#line 329 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_id_link("topic", "forum-topic", "/forum_topics/", { a1, a2 }); }}
	goto st1659;
tr838:
#line 389 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_bare_unnamed_url({ ts, te });
  }}
	goto st1659;
tr955:
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 385 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ g1, g2 }, { f1, f2 });
  }}
	goto st1659;
tr970:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 381 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
	goto st1659;
tr972:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 381 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
	goto st1659;
tr992:
#line 408 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_B, { ts, te }); }}
	goto st1659;
tr1008:
#line 410 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_I, { ts, te }); }}
	goto st1659;
tr1019:
#line 412 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_S, { ts, te }); }}
	goto st1659;
tr1037:
#line 414 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_U, { ts, te }); }}
	goto st1659;
tr1039:
#line 407 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_B, "<strong>"); }}
	goto st1659;
tr1040:
#line 430 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (header_mode) {
      append_html_escaped("<br>");
    } else {
      append("<br>");
    };
  }}
	goto st1659;
tr1051:
#line 470 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_inline_code({ a1, a2 });
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1885;}}
  }}
	goto st1659;
tr1052:
#line 470 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_inline_code({ a1, a2 });
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1885;}}
  }}
	goto st1659;
tr1054:
#line 465 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_inline_code();
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1885;}}
  }}
	goto st1659;
tr1055:
#line 465 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_inline_code();
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1885;}}
  }}
	goto st1659;
tr1249:
#line 409 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_I, "<em>"); }}
	goto st1659;
tr1257:
#line 496 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    dstack_open_element(INLINE_NODTEXT, "");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1889;}}
  }}
	goto st1659;
tr1258:
#line 496 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element(INLINE_NODTEXT, "");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1889;}}
  }}
	goto st1659;
tr1265:
#line 411 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_S, "<s>"); }}
	goto st1659;
tr1272:
#line 481 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element(INLINE_SPOILER, "<span class=\"spoiler\">");
  }}
	goto st1659;
tr1282:
#line 416 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element(INLINE_TN, "<span class=\"tn\">");
  }}
	goto st1659;
tr1284:
#line 413 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_U, "<u>"); }}
	goto st1659;
tr1312:
#line 377 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ b1, b2 }, { a1, a2 });
  }}
	goto st1659;
tr1416:
#line 393 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_unnamed_url({ a1, a2 });
  }}
	goto st1659;
tr1544:
#line 381 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
	goto st1659;
tr1570:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 393 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_unnamed_url({ a1, a2 });
  }}
	goto st1659;
tr1592:
#line 397 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_mention({ a1, a2 + 1 });
  }}
	goto st1659;
tr2071:
#line 596 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1659;
tr2078:
#line 586 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append(' ');
  }}
	goto st1659;
tr2102:
#line 596 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1659;
tr2103:
#line 592 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append({ ts, te });
  }}
	goto st1659;
tr2105:
#line 556 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("inline newline");

    if (header_mode) {
      dstack_close_leaf_blocks();
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    } else if (dstack_is_open(BLOCK_UL)) {
      dstack_close_list();
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    } else {
      append("<br>");
    }
  }}
	goto st1659;
tr2112:
#line 510 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_QUOTE);
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr2113:
#line 521 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_EXPAND);
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr2114:
#line 504 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr2115:
#line 538 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("inline newline2");

    if (dstack_check(BLOCK_P)) {
      dstack_rewind();
    } else if (header_mode) {
      dstack_close_leaf_blocks();
    } else {
      dstack_close_list();
    }

    if (options.f_inline) {
      append(" ");
    }

    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr2118:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 401 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("inline list");
    {( p) = (( ts + 1))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1659;
tr2122:
#line 373 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_bare_named_url({ b1, b2 + 1 }, { a1, a2 });
  }}
	goto st1659;
tr2134:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 365 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1659;
tr2136:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 365 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1659;
tr2138:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 369 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1659;
tr2140:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 369 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1659;
tr2144:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 361 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 });
  }}
	goto st1659;
tr2146:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 361 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 });
  }}
	goto st1659;
tr2148:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 357 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { d1, d2 });
  }}
	goto st1659;
tr2150:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 357 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { d1, d2 });
  }}
	goto st1659;
tr2162:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 340 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("alias", "tag-alias", "/tags/aliases/", { a1, a2 }); }}
	goto st1659;
tr2169:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 336 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("artist", "artist", "/artists/", { a1, a2 }); }}
	goto st1659;
tr2171:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 337 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("artist changes", "artist-changes-for", "/artists/versions?search[artist_id]=", { a1, a2 }); }}
	goto st1659;
tr2177:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 350 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("avoid posting", "avoid-posting", "/avoid_postings/", { a1, a2 }); }}
	goto st1659;
tr2183:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 338 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("ban", "ban", "/bans/", { a1, a2 }); }}
	goto st1659;
tr2187:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 339 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("BUR", "bulk-update-request", "/bulk_update_requests/", { a1, a2 }); }}
	goto st1659;
tr2197:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 331 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("comment", "comment", "/comments/", { a1, a2 }); }}
	goto st1659;
tr2201:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 354 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("commit", "github-commit", "https://github.com/PawsMovin/PawsMovin/commit/", { a1, a2 }); }}
	goto st1659;
tr2209:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 332 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("dmail", "dmail", "/dmails/", { a1, a2 }); }}
	goto st1659;
tr2212:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 333 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_dmail_key_link({ a1, a2 }, { b1, b2 }); }}
	goto st1659;
tr2216:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 349 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("avoid posting", "avoid-posting", "/avoid_postings/", { a1, a2 }); }}
	goto st1659;
tr2223:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 326 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("flag", "post-flag", "/posts/flags/", { a1, a2 }); }}
	goto st1659;
tr2230:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 328 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("forum", "forum-post", "/forum_posts/", { a1, a2 }); }}
	goto st1659;
tr2232:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 329 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("topic", "forum-topic", "/forum_topics/", { a1, a2 }); }}
	goto st1659;
tr2235:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 330 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_paged_link("topic #", { a1, a2 }, "<a class=\"dtext-link dtext-id-link dtext-forum-topic-id-link\" href=\"", "/forum_topics/", "?page=", { b1, b2 }); }}
	goto st1659;
tr2245:
#line 389 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_bare_unnamed_url({ ts, te });
  }}
	goto st1659;
tr2258:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 341 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("implication", "tag-implication", "/tags/implications/", { a1, a2 }); }}
	goto st1659;
tr2264:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 352 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("issue", "github", "https://github.com/PawsMovin/PawsMovin/issues/", { a1, a2 }); }}
	goto st1659;
tr2269:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 342 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("mod action", "mod-action", "/mod_actions/", { a1, a2 }); }}
	goto st1659;
tr2275:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 327 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("note", "note", "/notes/", { a1, a2 }); }}
	goto st1659;
tr2283:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 334 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("pool", "pool", "/pools/", { a1, a2 }); }}
	goto st1659;
tr2287:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 324 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("post", "post", "/posts/", { a1, a2 }); }}
	goto st1659;
tr2289:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 325 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("post changes", "post-changes-for", "/posts/versions?search[post_id]=", { a1, a2 }); }}
	goto st1659;
tr2294:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 353 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("pull", "github-pull", "https://github.com/PawsMovin/PawsMovin/pull/", { a1, a2 }); }}
	goto st1659;
tr2302:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 343 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("record", "user-feedback", "/users/feedbacks/", { a1, a2 }); }}
	goto st1659;
tr2307:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 346 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("set", "set", "/post_sets/", { a1, a2 }); }}
	goto st1659;
tr2317:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 348 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("takedown", "takedown", "/takedowns/", { a1, a2 }); }}
	goto st1659;
tr2326:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 306 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    if(posts.size() < options.max_thumbs) {
      long post_id = strtol(a1, (char**)&a2, 10);
      posts.push_back(post_id);
      append("<a class=\"dtext-link dtext-id-link dtext-post-id-link thumb-placeholder-link\" data-id=\"");
      append_html_escaped({ a1, a2 });
      append("\" href=\"");
      append_relative_url("/posts/");
      append_uri_escaped({ a1, a2 });
      append("\">");
      append("post #");
      append_html_escaped({ a1, a2 });
      append("</a>");
    } else {
      append_id_link("post", "post", "/posts/", { a1, a2 });
    }
  }}
	goto st1659;
tr2333:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 347 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("ticket", "ticket", "/tickets/", { a1, a2 }); }}
	goto st1659;
tr2342:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 335 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("user", "user", "/users/", { a1, a2 }); }}
	goto st1659;
tr2350:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 344 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("wiki", "wiki-page", "/wiki_pages/", { a1, a2 }); }}
	goto st1659;
tr2352:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 345 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("wiki changes", "wiki-page-changes-for", "/wiki_pages/versions?search[wiki_page_id]=", { a1, a2 }); }}
	goto st1659;
tr2378:
#line 459 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    if(options.f_allow_color) {
      dstack_close_element(INLINE_COLOR, { ts, te });
    }
  }}
	goto st1659;
tr2379:
#line 470 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_inline_code({ a1, a2 });
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1885;}}
  }}
	goto st1659;
tr2380:
#line 465 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_inline_code();
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1885;}}
  }}
	goto st1659;
tr2381:
#line 385 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_named_url({ g1, g2 }, { f1, f2 });
  }}
	goto st1659;
tr2382:
#line 496 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_open_element(INLINE_NODTEXT, "");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1659;goto st1889;}}
  }}
	goto st1659;
tr2402:
#line 397 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_mention({ a1, a2 + 1 });
  }}
	goto st1659;
st1659:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1659;
case 1659:
#line 1 "NONE"
	{( ts) = ( p);}
#line 7203 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) > 60 ) {
		if ( 64 <= (*( p)) && (*( p)) <= 64 ) {
			_widec = (short)(1152 + ((*( p)) - -128));
			if ( 
#line 88 "ext/dtext/dtext.cpp.rl"
 is_mention_boundary(p[-1])  ) _widec += 256;
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 512;
		}
	} else if ( (*( p)) >= 60 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 0: goto tr2075;
		case 9: goto tr2076;
		case 10: goto tr2077;
		case 13: goto tr2078;
		case 32: goto tr2076;
		case 34: goto tr2079;
		case 38: goto tr2080;
		case 65: goto tr2082;
		case 66: goto tr2083;
		case 67: goto tr2084;
		case 68: goto tr2085;
		case 70: goto tr2086;
		case 72: goto tr2087;
		case 73: goto tr2088;
		case 77: goto tr2089;
		case 78: goto tr2090;
		case 80: goto tr2091;
		case 82: goto tr2092;
		case 83: goto tr2093;
		case 84: goto tr2094;
		case 85: goto tr2095;
		case 87: goto tr2096;
		case 91: goto tr2097;
		case 97: goto tr2082;
		case 98: goto tr2083;
		case 99: goto tr2084;
		case 100: goto tr2085;
		case 102: goto tr2086;
		case 104: goto tr2087;
		case 105: goto tr2088;
		case 109: goto tr2089;
		case 110: goto tr2090;
		case 112: goto tr2091;
		case 114: goto tr2092;
		case 115: goto tr2093;
		case 116: goto tr2094;
		case 117: goto tr2095;
		case 119: goto tr2096;
		case 123: goto tr2098;
		case 828: goto tr2099;
		case 1084: goto tr2100;
		case 1344: goto tr2071;
		case 1600: goto tr2071;
		case 1856: goto tr2071;
		case 2112: goto tr2101;
	}
	if ( _widec < 48 ) {
		if ( _widec < -32 ) {
			if ( _widec > -63 ) {
				if ( -62 <= _widec && _widec <= -33 )
					goto st1660;
			} else
				goto tr2071;
		} else if ( _widec > -17 ) {
			if ( _widec > -12 ) {
				if ( -11 <= _widec && _widec <= 47 )
					goto tr2071;
			} else if ( _widec >= -16 )
				goto tr2074;
		} else
			goto tr2073;
	} else if ( _widec > 57 ) {
		if ( _widec < 69 ) {
			if ( _widec > 59 ) {
				if ( 61 <= _widec && _widec <= 63 )
					goto tr2071;
			} else if ( _widec >= 58 )
				goto tr2071;
		} else if ( _widec > 90 ) {
			if ( _widec < 101 ) {
				if ( 92 <= _widec && _widec <= 96 )
					goto tr2071;
			} else if ( _widec > 122 ) {
				if ( 124 <= _widec )
					goto tr2071;
			} else
				goto tr2081;
		} else
			goto tr2081;
	} else
		goto tr2081;
	goto st0;
st1660:
	if ( ++( p) == ( pe) )
		goto _test_eof1660;
case 1660:
	if ( (*( p)) <= -65 )
		goto tr235;
	goto tr2102;
tr235:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1661;
st1661:
	if ( ++( p) == ( pe) )
		goto _test_eof1661;
case 1661:
#line 7321 "ext/dtext/dtext.cpp"
	if ( (*( p)) < -32 ) {
		if ( -62 <= (*( p)) && (*( p)) <= -33 )
			goto st202;
	} else if ( (*( p)) > -17 ) {
		if ( -16 <= (*( p)) && (*( p)) <= -12 )
			goto st204;
	} else
		goto st203;
	goto tr2103;
st202:
	if ( ++( p) == ( pe) )
		goto _test_eof202;
case 202:
	if ( (*( p)) <= -65 )
		goto tr235;
	goto tr234;
st203:
	if ( ++( p) == ( pe) )
		goto _test_eof203;
case 203:
	if ( (*( p)) <= -65 )
		goto st202;
	goto tr234;
st204:
	if ( ++( p) == ( pe) )
		goto _test_eof204;
case 204:
	if ( (*( p)) <= -65 )
		goto st203;
	goto tr237;
tr2073:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1662;
st1662:
	if ( ++( p) == ( pe) )
		goto _test_eof1662;
case 1662:
#line 7362 "ext/dtext/dtext.cpp"
	if ( (*( p)) <= -65 )
		goto st202;
	goto tr2102;
tr2074:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1663;
st1663:
	if ( ++( p) == ( pe) )
		goto _test_eof1663;
case 1663:
#line 7376 "ext/dtext/dtext.cpp"
	if ( (*( p)) <= -65 )
		goto st203;
	goto tr2102;
tr239:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 538 "ext/dtext/dtext.cpp.rl"
	{( act) = 80;}
	goto st1664;
tr2075:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 590 "ext/dtext/dtext.cpp.rl"
	{( act) = 97;}
	goto st1664;
st1664:
	if ( ++( p) == ( pe) )
		goto _test_eof1664;
case 1664:
#line 7396 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr239;
		case 9: goto st205;
		case 10: goto tr239;
		case 32: goto st205;
	}
	goto tr234;
st205:
	if ( ++( p) == ( pe) )
		goto _test_eof205;
case 205:
	switch( (*( p)) ) {
		case 0: goto tr239;
		case 9: goto st205;
		case 10: goto tr239;
		case 32: goto st205;
	}
	goto tr234;
tr2076:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1665;
st1665:
	if ( ++( p) == ( pe) )
		goto _test_eof1665;
case 1665:
#line 7425 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st205;
		case 9: goto st206;
		case 10: goto st205;
		case 32: goto st206;
	}
	goto tr2102;
st206:
	if ( ++( p) == ( pe) )
		goto _test_eof206;
case 206:
	switch( (*( p)) ) {
		case 0: goto st205;
		case 9: goto st206;
		case 10: goto st205;
		case 32: goto st206;
	}
	goto tr241;
tr2077:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 556 "ext/dtext/dtext.cpp.rl"
	{( act) = 81;}
	goto st1666;
st1666:
	if ( ++( p) == ( pe) )
		goto _test_eof1666;
case 1666:
#line 7454 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr239;
		case 9: goto st207;
		case 10: goto tr2106;
		case 32: goto st207;
		case 42: goto tr2107;
		case 60: goto tr2108;
		case 72: goto st330;
		case 91: goto tr2110;
		case 96: goto st372;
		case 104: goto st330;
	}
	goto tr2105;
st207:
	if ( ++( p) == ( pe) )
		goto _test_eof207;
case 207:
	switch( (*( p)) ) {
		case 0: goto tr239;
		case 9: goto st207;
		case 10: goto tr239;
		case 32: goto st207;
		case 60: goto tr245;
		case 91: goto tr246;
	}
	goto tr243;
tr245:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st208;
st208:
	if ( ++( p) == ( pe) )
		goto _test_eof208;
case 208:
#line 7489 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st209;
		case 66: goto st234;
		case 72: goto st244;
		case 81: goto st239;
		case 83: goto st247;
		case 98: goto st234;
		case 104: goto st244;
		case 113: goto st239;
		case 115: goto st247;
	}
	goto tr243;
st209:
	if ( ++( p) == ( pe) )
		goto _test_eof209;
case 209:
	switch( (*( p)) ) {
		case 66: goto st210;
		case 69: goto st220;
		case 81: goto st226;
		case 84: goto st231;
		case 98: goto st210;
		case 101: goto st220;
		case 113: goto st226;
		case 116: goto st231;
	}
	goto tr243;
st210:
	if ( ++( p) == ( pe) )
		goto _test_eof210;
case 210:
	switch( (*( p)) ) {
		case 76: goto st211;
		case 108: goto st211;
	}
	goto tr243;
st211:
	if ( ++( p) == ( pe) )
		goto _test_eof211;
case 211:
	switch( (*( p)) ) {
		case 79: goto st212;
		case 111: goto st212;
	}
	goto tr234;
st212:
	if ( ++( p) == ( pe) )
		goto _test_eof212;
case 212:
	switch( (*( p)) ) {
		case 67: goto st213;
		case 99: goto st213;
	}
	goto tr234;
st213:
	if ( ++( p) == ( pe) )
		goto _test_eof213;
case 213:
	switch( (*( p)) ) {
		case 75: goto st214;
		case 107: goto st214;
	}
	goto tr234;
st214:
	if ( ++( p) == ( pe) )
		goto _test_eof214;
case 214:
	switch( (*( p)) ) {
		case 81: goto st215;
		case 113: goto st215;
	}
	goto tr234;
st215:
	if ( ++( p) == ( pe) )
		goto _test_eof215;
case 215:
	switch( (*( p)) ) {
		case 85: goto st216;
		case 117: goto st216;
	}
	goto tr234;
st216:
	if ( ++( p) == ( pe) )
		goto _test_eof216;
case 216:
	switch( (*( p)) ) {
		case 79: goto st217;
		case 111: goto st217;
	}
	goto tr234;
st217:
	if ( ++( p) == ( pe) )
		goto _test_eof217;
case 217:
	switch( (*( p)) ) {
		case 84: goto st218;
		case 116: goto st218;
	}
	goto tr234;
st218:
	if ( ++( p) == ( pe) )
		goto _test_eof218;
case 218:
	switch( (*( p)) ) {
		case 69: goto st219;
		case 101: goto st219;
	}
	goto tr234;
st219:
	if ( ++( p) == ( pe) )
		goto _test_eof219;
case 219:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2653 )
		goto st1667;
	goto tr234;
st1667:
	if ( ++( p) == ( pe) )
		goto _test_eof1667;
case 1667:
	switch( (*( p)) ) {
		case 9: goto st1667;
		case 32: goto st1667;
	}
	goto tr2112;
st220:
	if ( ++( p) == ( pe) )
		goto _test_eof220;
case 220:
	switch( (*( p)) ) {
		case 88: goto st221;
		case 120: goto st221;
	}
	goto tr243;
st221:
	if ( ++( p) == ( pe) )
		goto _test_eof221;
case 221:
	switch( (*( p)) ) {
		case 80: goto st222;
		case 112: goto st222;
	}
	goto tr234;
st222:
	if ( ++( p) == ( pe) )
		goto _test_eof222;
case 222:
	switch( (*( p)) ) {
		case 65: goto st223;
		case 97: goto st223;
	}
	goto tr234;
st223:
	if ( ++( p) == ( pe) )
		goto _test_eof223;
case 223:
	switch( (*( p)) ) {
		case 78: goto st224;
		case 110: goto st224;
	}
	goto tr234;
st224:
	if ( ++( p) == ( pe) )
		goto _test_eof224;
case 224:
	switch( (*( p)) ) {
		case 68: goto st225;
		case 100: goto st225;
	}
	goto tr234;
st225:
	if ( ++( p) == ( pe) )
		goto _test_eof225;
case 225:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_EXPAND)  ) _widec += 256;
	}
	if ( _widec == 3134 )
		goto st1668;
	goto tr234;
st1668:
	if ( ++( p) == ( pe) )
		goto _test_eof1668;
case 1668:
	switch( (*( p)) ) {
		case 9: goto st1668;
		case 32: goto st1668;
	}
	goto tr2113;
st226:
	if ( ++( p) == ( pe) )
		goto _test_eof226;
case 226:
	switch( (*( p)) ) {
		case 85: goto st227;
		case 117: goto st227;
	}
	goto tr234;
st227:
	if ( ++( p) == ( pe) )
		goto _test_eof227;
case 227:
	switch( (*( p)) ) {
		case 79: goto st228;
		case 111: goto st228;
	}
	goto tr234;
st228:
	if ( ++( p) == ( pe) )
		goto _test_eof228;
case 228:
	switch( (*( p)) ) {
		case 84: goto st229;
		case 116: goto st229;
	}
	goto tr234;
st229:
	if ( ++( p) == ( pe) )
		goto _test_eof229;
case 229:
	switch( (*( p)) ) {
		case 69: goto st230;
		case 101: goto st230;
	}
	goto tr234;
st230:
	if ( ++( p) == ( pe) )
		goto _test_eof230;
case 230:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2622 )
		goto st1667;
	goto tr234;
st231:
	if ( ++( p) == ( pe) )
		goto _test_eof231;
case 231:
	switch( (*( p)) ) {
		case 68: goto st232;
		case 72: goto st233;
		case 100: goto st232;
		case 104: goto st233;
	}
	goto tr243;
st232:
	if ( ++( p) == ( pe) )
		goto _test_eof232;
case 232:
	if ( (*( p)) == 62 )
		goto tr278;
	goto tr234;
st233:
	if ( ++( p) == ( pe) )
		goto _test_eof233;
case 233:
	if ( (*( p)) == 62 )
		goto tr279;
	goto tr234;
st234:
	if ( ++( p) == ( pe) )
		goto _test_eof234;
case 234:
	switch( (*( p)) ) {
		case 76: goto st235;
		case 108: goto st235;
	}
	goto tr243;
st235:
	if ( ++( p) == ( pe) )
		goto _test_eof235;
case 235:
	switch( (*( p)) ) {
		case 79: goto st236;
		case 111: goto st236;
	}
	goto tr234;
st236:
	if ( ++( p) == ( pe) )
		goto _test_eof236;
case 236:
	switch( (*( p)) ) {
		case 67: goto st237;
		case 99: goto st237;
	}
	goto tr234;
st237:
	if ( ++( p) == ( pe) )
		goto _test_eof237;
case 237:
	switch( (*( p)) ) {
		case 75: goto st238;
		case 107: goto st238;
	}
	goto tr234;
st238:
	if ( ++( p) == ( pe) )
		goto _test_eof238;
case 238:
	switch( (*( p)) ) {
		case 81: goto st239;
		case 113: goto st239;
	}
	goto tr234;
st239:
	if ( ++( p) == ( pe) )
		goto _test_eof239;
case 239:
	switch( (*( p)) ) {
		case 85: goto st240;
		case 117: goto st240;
	}
	goto tr234;
st240:
	if ( ++( p) == ( pe) )
		goto _test_eof240;
case 240:
	switch( (*( p)) ) {
		case 79: goto st241;
		case 111: goto st241;
	}
	goto tr234;
st241:
	if ( ++( p) == ( pe) )
		goto _test_eof241;
case 241:
	switch( (*( p)) ) {
		case 84: goto st242;
		case 116: goto st242;
	}
	goto tr234;
st242:
	if ( ++( p) == ( pe) )
		goto _test_eof242;
case 242:
	switch( (*( p)) ) {
		case 69: goto st243;
		case 101: goto st243;
	}
	goto tr234;
st243:
	if ( ++( p) == ( pe) )
		goto _test_eof243;
case 243:
	if ( (*( p)) == 62 )
		goto tr288;
	goto tr234;
st244:
	if ( ++( p) == ( pe) )
		goto _test_eof244;
case 244:
	switch( (*( p)) ) {
		case 82: goto st245;
		case 114: goto st245;
	}
	goto tr243;
st245:
	if ( ++( p) == ( pe) )
		goto _test_eof245;
case 245:
	if ( (*( p)) == 62 )
		goto st246;
	goto tr243;
st246:
	if ( ++( p) == ( pe) )
		goto _test_eof246;
case 246:
	switch( (*( p)) ) {
		case 0: goto st1669;
		case 9: goto st246;
		case 10: goto st1669;
		case 32: goto st246;
	}
	goto tr243;
st1669:
	if ( ++( p) == ( pe) )
		goto _test_eof1669;
case 1669:
	switch( (*( p)) ) {
		case 0: goto st1669;
		case 10: goto st1669;
	}
	goto tr2114;
st247:
	if ( ++( p) == ( pe) )
		goto _test_eof247;
case 247:
	switch( (*( p)) ) {
		case 80: goto st248;
		case 112: goto st248;
	}
	goto tr243;
st248:
	if ( ++( p) == ( pe) )
		goto _test_eof248;
case 248:
	switch( (*( p)) ) {
		case 79: goto st249;
		case 111: goto st249;
	}
	goto tr243;
st249:
	if ( ++( p) == ( pe) )
		goto _test_eof249;
case 249:
	switch( (*( p)) ) {
		case 73: goto st250;
		case 105: goto st250;
	}
	goto tr243;
st250:
	if ( ++( p) == ( pe) )
		goto _test_eof250;
case 250:
	switch( (*( p)) ) {
		case 76: goto st251;
		case 108: goto st251;
	}
	goto tr243;
st251:
	if ( ++( p) == ( pe) )
		goto _test_eof251;
case 251:
	switch( (*( p)) ) {
		case 69: goto st252;
		case 101: goto st252;
	}
	goto tr243;
st252:
	if ( ++( p) == ( pe) )
		goto _test_eof252;
case 252:
	switch( (*( p)) ) {
		case 82: goto st253;
		case 114: goto st253;
	}
	goto tr243;
st253:
	if ( ++( p) == ( pe) )
		goto _test_eof253;
case 253:
	switch( (*( p)) ) {
		case 62: goto st254;
		case 83: goto st255;
		case 115: goto st255;
	}
	goto tr243;
st254:
	if ( ++( p) == ( pe) )
		goto _test_eof254;
case 254:
	switch( (*( p)) ) {
		case 0: goto tr300;
		case 9: goto st254;
		case 10: goto tr300;
		case 32: goto st254;
	}
	goto tr243;
st255:
	if ( ++( p) == ( pe) )
		goto _test_eof255;
case 255:
	if ( (*( p)) == 62 )
		goto st254;
	goto tr243;
tr246:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st256;
st256:
	if ( ++( p) == ( pe) )
		goto _test_eof256;
case 256:
#line 7978 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st257;
		case 72: goto st267;
		case 81: goto st269;
		case 83: goto st274;
		case 104: goto st267;
		case 113: goto st269;
		case 115: goto st274;
	}
	goto tr243;
st257:
	if ( ++( p) == ( pe) )
		goto _test_eof257;
case 257:
	switch( (*( p)) ) {
		case 69: goto st258;
		case 81: goto st215;
		case 84: goto st264;
		case 101: goto st258;
		case 113: goto st215;
		case 116: goto st264;
	}
	goto tr243;
st258:
	if ( ++( p) == ( pe) )
		goto _test_eof258;
case 258:
	switch( (*( p)) ) {
		case 88: goto st259;
		case 120: goto st259;
	}
	goto tr243;
st259:
	if ( ++( p) == ( pe) )
		goto _test_eof259;
case 259:
	switch( (*( p)) ) {
		case 80: goto st260;
		case 112: goto st260;
	}
	goto tr243;
st260:
	if ( ++( p) == ( pe) )
		goto _test_eof260;
case 260:
	switch( (*( p)) ) {
		case 65: goto st261;
		case 97: goto st261;
	}
	goto tr243;
st261:
	if ( ++( p) == ( pe) )
		goto _test_eof261;
case 261:
	switch( (*( p)) ) {
		case 78: goto st262;
		case 110: goto st262;
	}
	goto tr243;
st262:
	if ( ++( p) == ( pe) )
		goto _test_eof262;
case 262:
	switch( (*( p)) ) {
		case 68: goto st263;
		case 100: goto st263;
	}
	goto tr243;
st263:
	if ( ++( p) == ( pe) )
		goto _test_eof263;
case 263:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_EXPAND)  ) _widec += 256;
	}
	if ( _widec == 3165 )
		goto st1668;
	goto tr243;
st264:
	if ( ++( p) == ( pe) )
		goto _test_eof264;
case 264:
	switch( (*( p)) ) {
		case 68: goto st265;
		case 72: goto st266;
		case 100: goto st265;
		case 104: goto st266;
	}
	goto tr243;
st265:
	if ( ++( p) == ( pe) )
		goto _test_eof265;
case 265:
	if ( (*( p)) == 93 )
		goto tr278;
	goto tr243;
st266:
	if ( ++( p) == ( pe) )
		goto _test_eof266;
case 266:
	if ( (*( p)) == 93 )
		goto tr279;
	goto tr243;
st267:
	if ( ++( p) == ( pe) )
		goto _test_eof267;
case 267:
	switch( (*( p)) ) {
		case 82: goto st268;
		case 114: goto st268;
	}
	goto tr243;
st268:
	if ( ++( p) == ( pe) )
		goto _test_eof268;
case 268:
	if ( (*( p)) == 93 )
		goto st246;
	goto tr243;
st269:
	if ( ++( p) == ( pe) )
		goto _test_eof269;
case 269:
	switch( (*( p)) ) {
		case 85: goto st270;
		case 117: goto st270;
	}
	goto tr243;
st270:
	if ( ++( p) == ( pe) )
		goto _test_eof270;
case 270:
	switch( (*( p)) ) {
		case 79: goto st271;
		case 111: goto st271;
	}
	goto tr243;
st271:
	if ( ++( p) == ( pe) )
		goto _test_eof271;
case 271:
	switch( (*( p)) ) {
		case 84: goto st272;
		case 116: goto st272;
	}
	goto tr243;
st272:
	if ( ++( p) == ( pe) )
		goto _test_eof272;
case 272:
	switch( (*( p)) ) {
		case 69: goto st273;
		case 101: goto st273;
	}
	goto tr243;
st273:
	if ( ++( p) == ( pe) )
		goto _test_eof273;
case 273:
	if ( (*( p)) == 93 )
		goto tr288;
	goto tr243;
st274:
	if ( ++( p) == ( pe) )
		goto _test_eof274;
case 274:
	switch( (*( p)) ) {
		case 80: goto st275;
		case 112: goto st275;
	}
	goto tr243;
st275:
	if ( ++( p) == ( pe) )
		goto _test_eof275;
case 275:
	switch( (*( p)) ) {
		case 79: goto st276;
		case 111: goto st276;
	}
	goto tr243;
st276:
	if ( ++( p) == ( pe) )
		goto _test_eof276;
case 276:
	switch( (*( p)) ) {
		case 73: goto st277;
		case 105: goto st277;
	}
	goto tr243;
st277:
	if ( ++( p) == ( pe) )
		goto _test_eof277;
case 277:
	switch( (*( p)) ) {
		case 76: goto st278;
		case 108: goto st278;
	}
	goto tr243;
st278:
	if ( ++( p) == ( pe) )
		goto _test_eof278;
case 278:
	switch( (*( p)) ) {
		case 69: goto st279;
		case 101: goto st279;
	}
	goto tr243;
st279:
	if ( ++( p) == ( pe) )
		goto _test_eof279;
case 279:
	switch( (*( p)) ) {
		case 82: goto st280;
		case 114: goto st280;
	}
	goto tr243;
st280:
	if ( ++( p) == ( pe) )
		goto _test_eof280;
case 280:
	switch( (*( p)) ) {
		case 83: goto st281;
		case 93: goto st254;
		case 115: goto st281;
	}
	goto tr243;
st281:
	if ( ++( p) == ( pe) )
		goto _test_eof281;
case 281:
	if ( (*( p)) == 93 )
		goto st254;
	goto tr243;
tr2106:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 538 "ext/dtext/dtext.cpp.rl"
	{( act) = 80;}
	goto st1670;
st1670:
	if ( ++( p) == ( pe) )
		goto _test_eof1670;
case 1670:
#line 8226 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr239;
		case 9: goto st205;
		case 10: goto tr2106;
		case 32: goto st205;
		case 60: goto st282;
		case 91: goto st286;
	}
	goto tr2115;
st282:
	if ( ++( p) == ( pe) )
		goto _test_eof282;
case 282:
	if ( (*( p)) == 47 )
		goto st283;
	goto tr326;
st283:
	if ( ++( p) == ( pe) )
		goto _test_eof283;
case 283:
	switch( (*( p)) ) {
		case 84: goto st284;
		case 116: goto st284;
	}
	goto tr326;
st284:
	if ( ++( p) == ( pe) )
		goto _test_eof284;
case 284:
	switch( (*( p)) ) {
		case 78: goto st285;
		case 110: goto st285;
	}
	goto tr326;
st285:
	if ( ++( p) == ( pe) )
		goto _test_eof285;
case 285:
	if ( (*( p)) == 62 )
		goto tr330;
	goto tr234;
st286:
	if ( ++( p) == ( pe) )
		goto _test_eof286;
case 286:
	if ( (*( p)) == 47 )
		goto st287;
	goto tr326;
st287:
	if ( ++( p) == ( pe) )
		goto _test_eof287;
case 287:
	switch( (*( p)) ) {
		case 84: goto st288;
		case 116: goto st288;
	}
	goto tr326;
st288:
	if ( ++( p) == ( pe) )
		goto _test_eof288;
case 288:
	switch( (*( p)) ) {
		case 78: goto st289;
		case 110: goto st289;
	}
	goto tr326;
st289:
	if ( ++( p) == ( pe) )
		goto _test_eof289;
case 289:
	if ( (*( p)) == 93 )
		goto tr330;
	goto tr234;
tr2107:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st290;
st290:
	if ( ++( p) == ( pe) )
		goto _test_eof290;
case 290:
#line 8308 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr334;
		case 32: goto tr334;
		case 42: goto st290;
	}
	goto tr243;
tr334:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
	goto st291;
st291:
	if ( ++( p) == ( pe) )
		goto _test_eof291;
case 291:
#line 8323 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr243;
		case 9: goto tr337;
		case 10: goto tr243;
		case 13: goto tr243;
		case 32: goto tr337;
	}
	goto tr336;
tr336:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1671;
st1671:
	if ( ++( p) == ( pe) )
		goto _test_eof1671;
case 1671:
#line 8340 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2118;
		case 10: goto tr2118;
		case 13: goto tr2118;
	}
	goto st1671;
tr337:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1672;
st1672:
	if ( ++( p) == ( pe) )
		goto _test_eof1672;
case 1672:
#line 8355 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2118;
		case 9: goto tr337;
		case 10: goto tr2118;
		case 13: goto tr2118;
		case 32: goto tr337;
	}
	goto tr336;
tr2108:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st292;
st292:
	if ( ++( p) == ( pe) )
		goto _test_eof292;
case 292:
#line 8372 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st293;
		case 66: goto st234;
		case 67: goto st303;
		case 69: goto st310;
		case 72: goto st244;
		case 78: goto st319;
		case 81: goto st239;
		case 83: goto st247;
		case 84: goto st326;
		case 98: goto st234;
		case 99: goto st303;
		case 101: goto st310;
		case 104: goto st244;
		case 110: goto st319;
		case 113: goto st239;
		case 115: goto st247;
		case 116: goto st326;
	}
	goto tr243;
st293:
	if ( ++( p) == ( pe) )
		goto _test_eof293;
case 293:
	switch( (*( p)) ) {
		case 66: goto st210;
		case 69: goto st220;
		case 81: goto st226;
		case 83: goto st294;
		case 84: goto st302;
		case 98: goto st210;
		case 101: goto st220;
		case 113: goto st226;
		case 115: goto st294;
		case 116: goto st302;
	}
	goto tr243;
st294:
	if ( ++( p) == ( pe) )
		goto _test_eof294;
case 294:
	switch( (*( p)) ) {
		case 80: goto st295;
		case 112: goto st295;
	}
	goto tr243;
st295:
	if ( ++( p) == ( pe) )
		goto _test_eof295;
case 295:
	switch( (*( p)) ) {
		case 79: goto st296;
		case 111: goto st296;
	}
	goto tr234;
st296:
	if ( ++( p) == ( pe) )
		goto _test_eof296;
case 296:
	switch( (*( p)) ) {
		case 73: goto st297;
		case 105: goto st297;
	}
	goto tr234;
st297:
	if ( ++( p) == ( pe) )
		goto _test_eof297;
case 297:
	switch( (*( p)) ) {
		case 76: goto st298;
		case 108: goto st298;
	}
	goto tr234;
st298:
	if ( ++( p) == ( pe) )
		goto _test_eof298;
case 298:
	switch( (*( p)) ) {
		case 69: goto st299;
		case 101: goto st299;
	}
	goto tr234;
st299:
	if ( ++( p) == ( pe) )
		goto _test_eof299;
case 299:
	switch( (*( p)) ) {
		case 82: goto st300;
		case 114: goto st300;
	}
	goto tr234;
st300:
	if ( ++( p) == ( pe) )
		goto _test_eof300;
case 300:
	switch( (*( p)) ) {
		case 62: goto tr351;
		case 83: goto st301;
		case 115: goto st301;
	}
	goto tr234;
st301:
	if ( ++( p) == ( pe) )
		goto _test_eof301;
case 301:
	if ( (*( p)) == 62 )
		goto tr351;
	goto tr234;
st302:
	if ( ++( p) == ( pe) )
		goto _test_eof302;
case 302:
	switch( (*( p)) ) {
		case 68: goto st232;
		case 72: goto st233;
		case 78: goto st285;
		case 100: goto st232;
		case 104: goto st233;
		case 110: goto st285;
	}
	goto tr234;
st303:
	if ( ++( p) == ( pe) )
		goto _test_eof303;
case 303:
	switch( (*( p)) ) {
		case 79: goto st304;
		case 111: goto st304;
	}
	goto tr243;
st304:
	if ( ++( p) == ( pe) )
		goto _test_eof304;
case 304:
	switch( (*( p)) ) {
		case 68: goto st305;
		case 100: goto st305;
	}
	goto tr243;
st305:
	if ( ++( p) == ( pe) )
		goto _test_eof305;
case 305:
	switch( (*( p)) ) {
		case 69: goto st306;
		case 101: goto st306;
	}
	goto tr243;
st306:
	if ( ++( p) == ( pe) )
		goto _test_eof306;
case 306:
	switch( (*( p)) ) {
		case 9: goto st307;
		case 32: goto st307;
		case 61: goto st308;
		case 62: goto tr358;
	}
	goto tr243;
st307:
	if ( ++( p) == ( pe) )
		goto _test_eof307;
case 307:
	switch( (*( p)) ) {
		case 9: goto st307;
		case 32: goto st307;
		case 61: goto st308;
	}
	goto tr243;
st308:
	if ( ++( p) == ( pe) )
		goto _test_eof308;
case 308:
	switch( (*( p)) ) {
		case 9: goto st308;
		case 32: goto st308;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr359;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr359;
	} else
		goto tr359;
	goto tr243;
tr359:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st309;
st309:
	if ( ++( p) == ( pe) )
		goto _test_eof309;
case 309:
#line 8567 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 62 )
		goto tr361;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st309;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st309;
	} else
		goto st309;
	goto tr243;
st310:
	if ( ++( p) == ( pe) )
		goto _test_eof310;
case 310:
	switch( (*( p)) ) {
		case 88: goto st311;
		case 120: goto st311;
	}
	goto tr243;
st311:
	if ( ++( p) == ( pe) )
		goto _test_eof311;
case 311:
	switch( (*( p)) ) {
		case 80: goto st312;
		case 112: goto st312;
	}
	goto tr243;
st312:
	if ( ++( p) == ( pe) )
		goto _test_eof312;
case 312:
	switch( (*( p)) ) {
		case 65: goto st313;
		case 97: goto st313;
	}
	goto tr243;
st313:
	if ( ++( p) == ( pe) )
		goto _test_eof313;
case 313:
	switch( (*( p)) ) {
		case 78: goto st314;
		case 110: goto st314;
	}
	goto tr243;
st314:
	if ( ++( p) == ( pe) )
		goto _test_eof314;
case 314:
	switch( (*( p)) ) {
		case 68: goto st315;
		case 100: goto st315;
	}
	goto tr243;
st315:
	if ( ++( p) == ( pe) )
		goto _test_eof315;
case 315:
	switch( (*( p)) ) {
		case 9: goto st316;
		case 32: goto st316;
		case 61: goto st318;
		case 62: goto tr358;
	}
	goto tr243;
tr370:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st316;
st316:
	if ( ++( p) == ( pe) )
		goto _test_eof316;
case 316:
#line 8643 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr243;
		case 9: goto tr370;
		case 10: goto tr243;
		case 13: goto tr243;
		case 32: goto tr370;
		case 61: goto tr371;
		case 62: goto tr372;
	}
	goto tr369;
tr369:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st317;
st317:
	if ( ++( p) == ( pe) )
		goto _test_eof317;
case 317:
#line 8662 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr243;
		case 10: goto tr243;
		case 13: goto tr243;
		case 62: goto tr361;
	}
	goto st317;
tr371:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st318;
st318:
	if ( ++( p) == ( pe) )
		goto _test_eof318;
case 318:
#line 8678 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr243;
		case 9: goto tr371;
		case 10: goto tr243;
		case 13: goto tr243;
		case 32: goto tr371;
		case 62: goto tr372;
	}
	goto tr369;
st319:
	if ( ++( p) == ( pe) )
		goto _test_eof319;
case 319:
	switch( (*( p)) ) {
		case 79: goto st320;
		case 111: goto st320;
	}
	goto tr243;
st320:
	if ( ++( p) == ( pe) )
		goto _test_eof320;
case 320:
	switch( (*( p)) ) {
		case 68: goto st321;
		case 100: goto st321;
	}
	goto tr243;
st321:
	if ( ++( p) == ( pe) )
		goto _test_eof321;
case 321:
	switch( (*( p)) ) {
		case 84: goto st322;
		case 116: goto st322;
	}
	goto tr243;
st322:
	if ( ++( p) == ( pe) )
		goto _test_eof322;
case 322:
	switch( (*( p)) ) {
		case 69: goto st323;
		case 101: goto st323;
	}
	goto tr243;
st323:
	if ( ++( p) == ( pe) )
		goto _test_eof323;
case 323:
	switch( (*( p)) ) {
		case 88: goto st324;
		case 120: goto st324;
	}
	goto tr243;
st324:
	if ( ++( p) == ( pe) )
		goto _test_eof324;
case 324:
	switch( (*( p)) ) {
		case 84: goto st325;
		case 116: goto st325;
	}
	goto tr243;
st325:
	if ( ++( p) == ( pe) )
		goto _test_eof325;
case 325:
	if ( (*( p)) == 62 )
		goto tr358;
	goto tr243;
st326:
	if ( ++( p) == ( pe) )
		goto _test_eof326;
case 326:
	switch( (*( p)) ) {
		case 65: goto st327;
		case 97: goto st327;
	}
	goto tr243;
st327:
	if ( ++( p) == ( pe) )
		goto _test_eof327;
case 327:
	switch( (*( p)) ) {
		case 66: goto st328;
		case 98: goto st328;
	}
	goto tr243;
st328:
	if ( ++( p) == ( pe) )
		goto _test_eof328;
case 328:
	switch( (*( p)) ) {
		case 76: goto st329;
		case 108: goto st329;
	}
	goto tr243;
st329:
	if ( ++( p) == ( pe) )
		goto _test_eof329;
case 329:
	switch( (*( p)) ) {
		case 69: goto st325;
		case 101: goto st325;
	}
	goto tr243;
st330:
	if ( ++( p) == ( pe) )
		goto _test_eof330;
case 330:
	if ( 49 <= (*( p)) && (*( p)) <= 54 )
		goto tr383;
	goto tr243;
tr383:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st331;
st331:
	if ( ++( p) == ( pe) )
		goto _test_eof331;
case 331:
#line 8800 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr384;
		case 46: goto tr385;
	}
	goto tr243;
tr384:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st332;
st332:
	if ( ++( p) == ( pe) )
		goto _test_eof332;
case 332:
#line 8814 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto tr386;
		case 35: goto tr386;
		case 38: goto tr386;
		case 45: goto tr386;
		case 95: goto tr386;
	}
	if ( (*( p)) < 65 ) {
		if ( 47 <= (*( p)) && (*( p)) <= 58 )
			goto tr386;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr386;
	} else
		goto tr386;
	goto tr243;
tr386:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st333;
st333:
	if ( ++( p) == ( pe) )
		goto _test_eof333;
case 333:
#line 8839 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto st333;
		case 35: goto st333;
		case 38: goto st333;
		case 46: goto tr388;
		case 95: goto st333;
	}
	if ( (*( p)) < 65 ) {
		if ( 45 <= (*( p)) && (*( p)) <= 58 )
			goto st333;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st333;
	} else
		goto st333;
	goto tr243;
tr385:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1673;
tr388:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1673;
st1673:
	if ( ++( p) == ( pe) )
		goto _test_eof1673;
case 1673:
#line 8872 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1673;
		case 32: goto st1673;
	}
	goto tr2114;
tr2110:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st334;
st334:
	if ( ++( p) == ( pe) )
		goto _test_eof334;
case 334:
#line 8886 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st335;
		case 67: goto st345;
		case 69: goto st352;
		case 72: goto st267;
		case 78: goto st361;
		case 81: goto st269;
		case 83: goto st274;
		case 84: goto st368;
		case 99: goto st345;
		case 101: goto st352;
		case 104: goto st267;
		case 110: goto st361;
		case 113: goto st269;
		case 115: goto st274;
		case 116: goto st368;
	}
	goto tr243;
st335:
	if ( ++( p) == ( pe) )
		goto _test_eof335;
case 335:
	switch( (*( p)) ) {
		case 69: goto st258;
		case 81: goto st215;
		case 83: goto st336;
		case 84: goto st344;
		case 101: goto st258;
		case 113: goto st215;
		case 115: goto st336;
		case 116: goto st344;
	}
	goto tr243;
st336:
	if ( ++( p) == ( pe) )
		goto _test_eof336;
case 336:
	switch( (*( p)) ) {
		case 80: goto st337;
		case 112: goto st337;
	}
	goto tr243;
st337:
	if ( ++( p) == ( pe) )
		goto _test_eof337;
case 337:
	switch( (*( p)) ) {
		case 79: goto st338;
		case 111: goto st338;
	}
	goto tr243;
st338:
	if ( ++( p) == ( pe) )
		goto _test_eof338;
case 338:
	switch( (*( p)) ) {
		case 73: goto st339;
		case 105: goto st339;
	}
	goto tr243;
st339:
	if ( ++( p) == ( pe) )
		goto _test_eof339;
case 339:
	switch( (*( p)) ) {
		case 76: goto st340;
		case 108: goto st340;
	}
	goto tr243;
st340:
	if ( ++( p) == ( pe) )
		goto _test_eof340;
case 340:
	switch( (*( p)) ) {
		case 69: goto st341;
		case 101: goto st341;
	}
	goto tr243;
st341:
	if ( ++( p) == ( pe) )
		goto _test_eof341;
case 341:
	switch( (*( p)) ) {
		case 82: goto st342;
		case 114: goto st342;
	}
	goto tr243;
st342:
	if ( ++( p) == ( pe) )
		goto _test_eof342;
case 342:
	switch( (*( p)) ) {
		case 83: goto st343;
		case 93: goto tr351;
		case 115: goto st343;
	}
	goto tr243;
st343:
	if ( ++( p) == ( pe) )
		goto _test_eof343;
case 343:
	if ( (*( p)) == 93 )
		goto tr351;
	goto tr243;
st344:
	if ( ++( p) == ( pe) )
		goto _test_eof344;
case 344:
	switch( (*( p)) ) {
		case 68: goto st265;
		case 72: goto st266;
		case 78: goto st289;
		case 100: goto st265;
		case 104: goto st266;
		case 110: goto st289;
	}
	goto tr243;
st345:
	if ( ++( p) == ( pe) )
		goto _test_eof345;
case 345:
	switch( (*( p)) ) {
		case 79: goto st346;
		case 111: goto st346;
	}
	goto tr243;
st346:
	if ( ++( p) == ( pe) )
		goto _test_eof346;
case 346:
	switch( (*( p)) ) {
		case 68: goto st347;
		case 100: goto st347;
	}
	goto tr243;
st347:
	if ( ++( p) == ( pe) )
		goto _test_eof347;
case 347:
	switch( (*( p)) ) {
		case 69: goto st348;
		case 101: goto st348;
	}
	goto tr243;
st348:
	if ( ++( p) == ( pe) )
		goto _test_eof348;
case 348:
	switch( (*( p)) ) {
		case 9: goto st349;
		case 32: goto st349;
		case 61: goto st350;
		case 93: goto tr358;
	}
	goto tr243;
st349:
	if ( ++( p) == ( pe) )
		goto _test_eof349;
case 349:
	switch( (*( p)) ) {
		case 9: goto st349;
		case 32: goto st349;
		case 61: goto st350;
	}
	goto tr243;
st350:
	if ( ++( p) == ( pe) )
		goto _test_eof350;
case 350:
	switch( (*( p)) ) {
		case 9: goto st350;
		case 32: goto st350;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr408;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr408;
	} else
		goto tr408;
	goto tr243;
tr408:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st351;
st351:
	if ( ++( p) == ( pe) )
		goto _test_eof351;
case 351:
#line 9077 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto tr361;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st351;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st351;
	} else
		goto st351;
	goto tr243;
st352:
	if ( ++( p) == ( pe) )
		goto _test_eof352;
case 352:
	switch( (*( p)) ) {
		case 88: goto st353;
		case 120: goto st353;
	}
	goto tr243;
st353:
	if ( ++( p) == ( pe) )
		goto _test_eof353;
case 353:
	switch( (*( p)) ) {
		case 80: goto st354;
		case 112: goto st354;
	}
	goto tr243;
st354:
	if ( ++( p) == ( pe) )
		goto _test_eof354;
case 354:
	switch( (*( p)) ) {
		case 65: goto st355;
		case 97: goto st355;
	}
	goto tr243;
st355:
	if ( ++( p) == ( pe) )
		goto _test_eof355;
case 355:
	switch( (*( p)) ) {
		case 78: goto st356;
		case 110: goto st356;
	}
	goto tr243;
st356:
	if ( ++( p) == ( pe) )
		goto _test_eof356;
case 356:
	switch( (*( p)) ) {
		case 68: goto st357;
		case 100: goto st357;
	}
	goto tr243;
st357:
	if ( ++( p) == ( pe) )
		goto _test_eof357;
case 357:
	switch( (*( p)) ) {
		case 9: goto st358;
		case 32: goto st358;
		case 61: goto st360;
		case 93: goto tr358;
	}
	goto tr243;
tr418:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st358;
st358:
	if ( ++( p) == ( pe) )
		goto _test_eof358;
case 358:
#line 9153 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr243;
		case 9: goto tr418;
		case 10: goto tr243;
		case 13: goto tr243;
		case 32: goto tr418;
		case 61: goto tr419;
		case 93: goto tr372;
	}
	goto tr417;
tr417:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st359;
st359:
	if ( ++( p) == ( pe) )
		goto _test_eof359;
case 359:
#line 9172 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr243;
		case 10: goto tr243;
		case 13: goto tr243;
		case 93: goto tr361;
	}
	goto st359;
tr419:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st360;
st360:
	if ( ++( p) == ( pe) )
		goto _test_eof360;
case 360:
#line 9188 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr243;
		case 9: goto tr419;
		case 10: goto tr243;
		case 13: goto tr243;
		case 32: goto tr419;
		case 93: goto tr372;
	}
	goto tr417;
st361:
	if ( ++( p) == ( pe) )
		goto _test_eof361;
case 361:
	switch( (*( p)) ) {
		case 79: goto st362;
		case 111: goto st362;
	}
	goto tr243;
st362:
	if ( ++( p) == ( pe) )
		goto _test_eof362;
case 362:
	switch( (*( p)) ) {
		case 68: goto st363;
		case 100: goto st363;
	}
	goto tr243;
st363:
	if ( ++( p) == ( pe) )
		goto _test_eof363;
case 363:
	switch( (*( p)) ) {
		case 84: goto st364;
		case 116: goto st364;
	}
	goto tr243;
st364:
	if ( ++( p) == ( pe) )
		goto _test_eof364;
case 364:
	switch( (*( p)) ) {
		case 69: goto st365;
		case 101: goto st365;
	}
	goto tr243;
st365:
	if ( ++( p) == ( pe) )
		goto _test_eof365;
case 365:
	switch( (*( p)) ) {
		case 88: goto st366;
		case 120: goto st366;
	}
	goto tr243;
st366:
	if ( ++( p) == ( pe) )
		goto _test_eof366;
case 366:
	switch( (*( p)) ) {
		case 84: goto st367;
		case 116: goto st367;
	}
	goto tr243;
st367:
	if ( ++( p) == ( pe) )
		goto _test_eof367;
case 367:
	if ( (*( p)) == 93 )
		goto tr358;
	goto tr243;
st368:
	if ( ++( p) == ( pe) )
		goto _test_eof368;
case 368:
	switch( (*( p)) ) {
		case 65: goto st369;
		case 97: goto st369;
	}
	goto tr243;
st369:
	if ( ++( p) == ( pe) )
		goto _test_eof369;
case 369:
	switch( (*( p)) ) {
		case 66: goto st370;
		case 98: goto st370;
	}
	goto tr243;
st370:
	if ( ++( p) == ( pe) )
		goto _test_eof370;
case 370:
	switch( (*( p)) ) {
		case 76: goto st371;
		case 108: goto st371;
	}
	goto tr243;
st371:
	if ( ++( p) == ( pe) )
		goto _test_eof371;
case 371:
	switch( (*( p)) ) {
		case 69: goto st367;
		case 101: goto st367;
	}
	goto tr243;
st372:
	if ( ++( p) == ( pe) )
		goto _test_eof372;
case 372:
	if ( (*( p)) == 96 )
		goto st373;
	goto tr243;
st373:
	if ( ++( p) == ( pe) )
		goto _test_eof373;
case 373:
	if ( (*( p)) == 96 )
		goto st374;
	goto tr243;
tr433:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st374;
st374:
	if ( ++( p) == ( pe) )
		goto _test_eof374;
case 374:
#line 9319 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr432;
		case 9: goto tr433;
		case 10: goto tr432;
		case 32: goto tr433;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr434;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr434;
	} else
		goto tr434;
	goto tr243;
tr442:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st375;
tr432:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st375;
st375:
	if ( ++( p) == ( pe) )
		goto _test_eof375;
case 375:
#line 9349 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr436;
		case 10: goto tr436;
	}
	goto tr435;
tr435:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st376;
st376:
	if ( ++( p) == ( pe) )
		goto _test_eof376;
case 376:
#line 9363 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr438;
		case 10: goto tr438;
	}
	goto st376;
tr438:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st377;
tr436:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st377;
st377:
	if ( ++( p) == ( pe) )
		goto _test_eof377;
case 377:
#line 9383 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr438;
		case 10: goto tr438;
		case 96: goto st378;
	}
	goto st376;
st378:
	if ( ++( p) == ( pe) )
		goto _test_eof378;
case 378:
	switch( (*( p)) ) {
		case 0: goto tr438;
		case 10: goto tr438;
		case 96: goto st379;
	}
	goto st376;
st379:
	if ( ++( p) == ( pe) )
		goto _test_eof379;
case 379:
	switch( (*( p)) ) {
		case 0: goto tr438;
		case 10: goto tr438;
		case 96: goto st380;
	}
	goto st376;
st380:
	if ( ++( p) == ( pe) )
		goto _test_eof380;
case 380:
	switch( (*( p)) ) {
		case 0: goto tr358;
		case 9: goto st380;
		case 10: goto tr358;
		case 32: goto st380;
	}
	goto st376;
tr434:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st381;
st381:
	if ( ++( p) == ( pe) )
		goto _test_eof381;
case 381:
#line 9429 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr442;
		case 9: goto tr443;
		case 10: goto tr442;
		case 32: goto tr443;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st381;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st381;
	} else
		goto st381;
	goto tr243;
tr443:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st382;
st382:
	if ( ++( p) == ( pe) )
		goto _test_eof382;
case 382:
#line 9453 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st375;
		case 9: goto st382;
		case 10: goto st375;
		case 32: goto st382;
	}
	goto tr243;
tr2079:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1674;
st1674:
	if ( ++( p) == ( pe) )
		goto _test_eof1674;
case 1674:
#line 9471 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 34 )
		goto tr2102;
	goto tr2121;
tr2121:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st383;
st383:
	if ( ++( p) == ( pe) )
		goto _test_eof383;
case 383:
#line 9483 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 34 )
		goto tr448;
	goto st383;
tr448:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st384;
st384:
	if ( ++( p) == ( pe) )
		goto _test_eof384;
case 384:
#line 9495 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 58 )
		goto st385;
	goto tr241;
st385:
	if ( ++( p) == ( pe) )
		goto _test_eof385;
case 385:
	switch( (*( p)) ) {
		case 35: goto tr450;
		case 47: goto tr451;
		case 72: goto tr452;
		case 91: goto st444;
		case 104: goto tr452;
	}
	goto tr241;
tr450:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1675;
tr455:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1675;
st1675:
	if ( ++( p) == ( pe) )
		goto _test_eof1675;
case 1675:
#line 9529 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st388;
		case -29: goto st390;
		case -17: goto st392;
		case 32: goto tr2122;
		case 34: goto st396;
		case 35: goto tr2122;
		case 39: goto st396;
		case 44: goto st396;
		case 46: goto st396;
		case 60: goto tr2122;
		case 62: goto tr2122;
		case 63: goto st396;
		case 91: goto tr2122;
		case 93: goto tr2122;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2122;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st387;
		} else
			goto st386;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2122;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st396;
		} else
			goto tr2122;
	} else
		goto st395;
	goto tr455;
st386:
	if ( ++( p) == ( pe) )
		goto _test_eof386;
case 386:
	if ( (*( p)) <= -65 )
		goto tr455;
	goto tr454;
st387:
	if ( ++( p) == ( pe) )
		goto _test_eof387;
case 387:
	if ( (*( p)) <= -65 )
		goto st386;
	goto tr454;
st388:
	if ( ++( p) == ( pe) )
		goto _test_eof388;
case 388:
	if ( (*( p)) == -99 )
		goto st389;
	if ( (*( p)) <= -65 )
		goto st386;
	goto tr454;
st389:
	if ( ++( p) == ( pe) )
		goto _test_eof389;
case 389:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr455;
	} else
		goto tr455;
	goto tr454;
st390:
	if ( ++( p) == ( pe) )
		goto _test_eof390;
case 390:
	if ( (*( p)) == -128 )
		goto st391;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st386;
	goto tr454;
st391:
	if ( ++( p) == ( pe) )
		goto _test_eof391;
case 391:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr455;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr455;
	} else
		goto tr455;
	goto tr454;
st392:
	if ( ++( p) == ( pe) )
		goto _test_eof392;
case 392:
	switch( (*( p)) ) {
		case -68: goto st393;
		case -67: goto st394;
	}
	if ( (*( p)) <= -65 )
		goto st386;
	goto tr454;
st393:
	if ( ++( p) == ( pe) )
		goto _test_eof393;
case 393:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr455;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr455;
	} else
		goto tr455;
	goto tr454;
st394:
	if ( ++( p) == ( pe) )
		goto _test_eof394;
case 394:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr455;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr455;
		} else if ( (*( p)) >= -95 )
			goto tr455;
	} else
		goto tr455;
	goto tr454;
st395:
	if ( ++( p) == ( pe) )
		goto _test_eof395;
case 395:
	if ( (*( p)) <= -65 )
		goto st387;
	goto tr454;
st396:
	if ( ++( p) == ( pe) )
		goto _test_eof396;
case 396:
	switch( (*( p)) ) {
		case -30: goto st388;
		case -29: goto st390;
		case -17: goto st392;
		case 32: goto tr454;
		case 34: goto st396;
		case 35: goto tr454;
		case 39: goto st396;
		case 44: goto st396;
		case 46: goto st396;
		case 60: goto tr454;
		case 62: goto tr454;
		case 63: goto st396;
		case 91: goto tr454;
		case 93: goto tr454;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr454;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st387;
		} else
			goto st386;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr454;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st396;
		} else
			goto tr454;
	} else
		goto st395;
	goto tr455;
tr451:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 373 "ext/dtext/dtext.cpp.rl"
	{( act) = 46;}
	goto st1676;
tr467:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 373 "ext/dtext/dtext.cpp.rl"
	{( act) = 46;}
	goto st1676;
st1676:
	if ( ++( p) == ( pe) )
		goto _test_eof1676;
case 1676:
#line 9732 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st399;
		case -29: goto st401;
		case -17: goto st403;
		case 32: goto tr2122;
		case 34: goto st407;
		case 35: goto tr455;
		case 39: goto st407;
		case 44: goto st407;
		case 46: goto st407;
		case 60: goto tr2122;
		case 62: goto tr2122;
		case 63: goto st408;
		case 91: goto tr2122;
		case 93: goto tr2122;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2122;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st398;
		} else
			goto st397;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2122;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st407;
		} else
			goto tr2122;
	} else
		goto st406;
	goto tr467;
st397:
	if ( ++( p) == ( pe) )
		goto _test_eof397;
case 397:
	if ( (*( p)) <= -65 )
		goto tr467;
	goto tr454;
st398:
	if ( ++( p) == ( pe) )
		goto _test_eof398;
case 398:
	if ( (*( p)) <= -65 )
		goto st397;
	goto tr454;
st399:
	if ( ++( p) == ( pe) )
		goto _test_eof399;
case 399:
	if ( (*( p)) == -99 )
		goto st400;
	if ( (*( p)) <= -65 )
		goto st397;
	goto tr454;
st400:
	if ( ++( p) == ( pe) )
		goto _test_eof400;
case 400:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr467;
	} else
		goto tr467;
	goto tr454;
st401:
	if ( ++( p) == ( pe) )
		goto _test_eof401;
case 401:
	if ( (*( p)) == -128 )
		goto st402;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st397;
	goto tr454;
st402:
	if ( ++( p) == ( pe) )
		goto _test_eof402;
case 402:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr467;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr467;
	} else
		goto tr467;
	goto tr454;
st403:
	if ( ++( p) == ( pe) )
		goto _test_eof403;
case 403:
	switch( (*( p)) ) {
		case -68: goto st404;
		case -67: goto st405;
	}
	if ( (*( p)) <= -65 )
		goto st397;
	goto tr454;
st404:
	if ( ++( p) == ( pe) )
		goto _test_eof404;
case 404:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr467;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr467;
	} else
		goto tr467;
	goto tr454;
st405:
	if ( ++( p) == ( pe) )
		goto _test_eof405;
case 405:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr467;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr467;
		} else if ( (*( p)) >= -95 )
			goto tr467;
	} else
		goto tr467;
	goto tr454;
st406:
	if ( ++( p) == ( pe) )
		goto _test_eof406;
case 406:
	if ( (*( p)) <= -65 )
		goto st398;
	goto tr454;
st407:
	if ( ++( p) == ( pe) )
		goto _test_eof407;
case 407:
	switch( (*( p)) ) {
		case -30: goto st399;
		case -29: goto st401;
		case -17: goto st403;
		case 32: goto tr454;
		case 34: goto st407;
		case 35: goto tr455;
		case 39: goto st407;
		case 44: goto st407;
		case 46: goto st407;
		case 60: goto tr454;
		case 62: goto tr454;
		case 63: goto st408;
		case 91: goto tr454;
		case 93: goto tr454;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr454;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st398;
		} else
			goto st397;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr454;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st407;
		} else
			goto tr454;
	} else
		goto st406;
	goto tr467;
st408:
	if ( ++( p) == ( pe) )
		goto _test_eof408;
case 408:
	switch( (*( p)) ) {
		case -30: goto st411;
		case -29: goto st413;
		case -17: goto st415;
		case 32: goto tr234;
		case 34: goto st408;
		case 35: goto tr455;
		case 39: goto st408;
		case 44: goto st408;
		case 46: goto st408;
		case 63: goto st408;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr234;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st410;
		} else
			goto st409;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr234;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st408;
		} else
			goto tr234;
	} else
		goto st418;
	goto tr486;
st409:
	if ( ++( p) == ( pe) )
		goto _test_eof409;
case 409:
	if ( (*( p)) <= -65 )
		goto tr486;
	goto tr234;
tr486:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 373 "ext/dtext/dtext.cpp.rl"
	{( act) = 46;}
	goto st1677;
st1677:
	if ( ++( p) == ( pe) )
		goto _test_eof1677;
case 1677:
#line 9969 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st411;
		case -29: goto st413;
		case -17: goto st415;
		case 32: goto tr2122;
		case 34: goto st408;
		case 35: goto tr455;
		case 39: goto st408;
		case 44: goto st408;
		case 46: goto st408;
		case 63: goto st408;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2122;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st410;
		} else
			goto st409;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2122;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st408;
		} else
			goto tr2122;
	} else
		goto st418;
	goto tr486;
st410:
	if ( ++( p) == ( pe) )
		goto _test_eof410;
case 410:
	if ( (*( p)) <= -65 )
		goto st409;
	goto tr234;
st411:
	if ( ++( p) == ( pe) )
		goto _test_eof411;
case 411:
	if ( (*( p)) == -99 )
		goto st412;
	if ( (*( p)) <= -65 )
		goto st409;
	goto tr234;
st412:
	if ( ++( p) == ( pe) )
		goto _test_eof412;
case 412:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr486;
	} else
		goto tr486;
	goto tr234;
st413:
	if ( ++( p) == ( pe) )
		goto _test_eof413;
case 413:
	if ( (*( p)) == -128 )
		goto st414;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st409;
	goto tr234;
st414:
	if ( ++( p) == ( pe) )
		goto _test_eof414;
case 414:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr486;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr486;
	} else
		goto tr486;
	goto tr234;
st415:
	if ( ++( p) == ( pe) )
		goto _test_eof415;
case 415:
	switch( (*( p)) ) {
		case -68: goto st416;
		case -67: goto st417;
	}
	if ( (*( p)) <= -65 )
		goto st409;
	goto tr234;
st416:
	if ( ++( p) == ( pe) )
		goto _test_eof416;
case 416:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr486;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr486;
	} else
		goto tr486;
	goto tr234;
st417:
	if ( ++( p) == ( pe) )
		goto _test_eof417;
case 417:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr486;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr486;
		} else if ( (*( p)) >= -95 )
			goto tr486;
	} else
		goto tr486;
	goto tr234;
st418:
	if ( ++( p) == ( pe) )
		goto _test_eof418;
case 418:
	if ( (*( p)) <= -65 )
		goto st410;
	goto tr234;
tr452:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st419;
st419:
	if ( ++( p) == ( pe) )
		goto _test_eof419;
case 419:
#line 10106 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st420;
		case 116: goto st420;
	}
	goto tr241;
st420:
	if ( ++( p) == ( pe) )
		goto _test_eof420;
case 420:
	switch( (*( p)) ) {
		case 84: goto st421;
		case 116: goto st421;
	}
	goto tr241;
st421:
	if ( ++( p) == ( pe) )
		goto _test_eof421;
case 421:
	switch( (*( p)) ) {
		case 80: goto st422;
		case 112: goto st422;
	}
	goto tr241;
st422:
	if ( ++( p) == ( pe) )
		goto _test_eof422;
case 422:
	switch( (*( p)) ) {
		case 58: goto st423;
		case 83: goto st443;
		case 115: goto st443;
	}
	goto tr241;
st423:
	if ( ++( p) == ( pe) )
		goto _test_eof423;
case 423:
	if ( (*( p)) == 47 )
		goto st424;
	goto tr241;
st424:
	if ( ++( p) == ( pe) )
		goto _test_eof424;
case 424:
	if ( (*( p)) == 47 )
		goto st425;
	goto tr241;
st425:
	if ( ++( p) == ( pe) )
		goto _test_eof425;
case 425:
	switch( (*( p)) ) {
		case 45: goto st427;
		case 95: goto st427;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st428;
		} else if ( (*( p)) >= -62 )
			goto st426;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st427;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st427;
		} else
			goto st427;
	} else
		goto st429;
	goto tr241;
st426:
	if ( ++( p) == ( pe) )
		goto _test_eof426;
case 426:
	if ( (*( p)) <= -65 )
		goto st427;
	goto tr241;
st427:
	if ( ++( p) == ( pe) )
		goto _test_eof427;
case 427:
	switch( (*( p)) ) {
		case 45: goto st427;
		case 46: goto st430;
		case 95: goto st427;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st428;
		} else if ( (*( p)) >= -62 )
			goto st426;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st427;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st427;
		} else
			goto st427;
	} else
		goto st429;
	goto tr241;
st428:
	if ( ++( p) == ( pe) )
		goto _test_eof428;
case 428:
	if ( (*( p)) <= -65 )
		goto st426;
	goto tr241;
st429:
	if ( ++( p) == ( pe) )
		goto _test_eof429;
case 429:
	if ( (*( p)) <= -65 )
		goto st428;
	goto tr241;
st430:
	if ( ++( p) == ( pe) )
		goto _test_eof430;
case 430:
	switch( (*( p)) ) {
		case -30: goto st433;
		case -29: goto st436;
		case -17: goto st438;
		case 45: goto tr509;
		case 95: goto tr509;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st432;
		} else if ( (*( p)) >= -62 )
			goto st431;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto tr509;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr509;
		} else
			goto tr509;
	} else
		goto st441;
	goto tr234;
st431:
	if ( ++( p) == ( pe) )
		goto _test_eof431;
case 431:
	if ( (*( p)) <= -65 )
		goto tr509;
	goto tr234;
tr509:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 373 "ext/dtext/dtext.cpp.rl"
	{( act) = 46;}
	goto st1678;
st1678:
	if ( ++( p) == ( pe) )
		goto _test_eof1678;
case 1678:
#line 10276 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st433;
		case -29: goto st436;
		case -17: goto st438;
		case 35: goto tr455;
		case 46: goto st430;
		case 47: goto tr467;
		case 58: goto st442;
		case 63: goto st408;
		case 95: goto tr509;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st432;
		} else if ( (*( p)) >= -62 )
			goto st431;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr509;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr509;
		} else
			goto tr509;
	} else
		goto st441;
	goto tr2122;
st432:
	if ( ++( p) == ( pe) )
		goto _test_eof432;
case 432:
	if ( (*( p)) <= -65 )
		goto st431;
	goto tr234;
st433:
	if ( ++( p) == ( pe) )
		goto _test_eof433;
case 433:
	if ( (*( p)) == -99 )
		goto st434;
	if ( (*( p)) <= -65 )
		goto st431;
	goto tr234;
st434:
	if ( ++( p) == ( pe) )
		goto _test_eof434;
case 434:
	if ( (*( p)) == -83 )
		goto st435;
	if ( (*( p)) <= -65 )
		goto tr509;
	goto tr234;
st435:
	if ( ++( p) == ( pe) )
		goto _test_eof435;
case 435:
	switch( (*( p)) ) {
		case -30: goto st433;
		case -29: goto st436;
		case -17: goto st438;
		case 35: goto tr455;
		case 46: goto st430;
		case 47: goto tr467;
		case 58: goto st442;
		case 63: goto st408;
		case 95: goto tr509;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st432;
		} else if ( (*( p)) >= -62 )
			goto st431;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr509;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr509;
		} else
			goto tr509;
	} else
		goto st441;
	goto tr234;
st436:
	if ( ++( p) == ( pe) )
		goto _test_eof436;
case 436:
	if ( (*( p)) == -128 )
		goto st437;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st431;
	goto tr234;
st437:
	if ( ++( p) == ( pe) )
		goto _test_eof437;
case 437:
	if ( (*( p)) < -120 ) {
		if ( (*( p)) > -126 ) {
			if ( -125 <= (*( p)) && (*( p)) <= -121 )
				goto tr509;
		} else
			goto st435;
	} else if ( (*( p)) > -111 ) {
		if ( (*( p)) < -108 ) {
			if ( -110 <= (*( p)) && (*( p)) <= -109 )
				goto tr509;
		} else if ( (*( p)) > -100 ) {
			if ( -99 <= (*( p)) && (*( p)) <= -65 )
				goto tr509;
		} else
			goto st435;
	} else
		goto st435;
	goto tr234;
st438:
	if ( ++( p) == ( pe) )
		goto _test_eof438;
case 438:
	switch( (*( p)) ) {
		case -68: goto st439;
		case -67: goto st440;
	}
	if ( (*( p)) <= -65 )
		goto st431;
	goto tr234;
st439:
	if ( ++( p) == ( pe) )
		goto _test_eof439;
case 439:
	switch( (*( p)) ) {
		case -119: goto st435;
		case -67: goto st435;
	}
	if ( (*( p)) <= -65 )
		goto tr509;
	goto tr234;
st440:
	if ( ++( p) == ( pe) )
		goto _test_eof440;
case 440:
	switch( (*( p)) ) {
		case -99: goto st435;
		case -96: goto st435;
		case -93: goto st435;
	}
	if ( (*( p)) <= -65 )
		goto tr509;
	goto tr234;
st441:
	if ( ++( p) == ( pe) )
		goto _test_eof441;
case 441:
	if ( (*( p)) <= -65 )
		goto st432;
	goto tr234;
st442:
	if ( ++( p) == ( pe) )
		goto _test_eof442;
case 442:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr516;
	goto tr234;
tr516:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 373 "ext/dtext/dtext.cpp.rl"
	{( act) = 46;}
	goto st1679;
st1679:
	if ( ++( p) == ( pe) )
		goto _test_eof1679;
case 1679:
#line 10455 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr455;
		case 47: goto tr467;
		case 63: goto st408;
	}
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr516;
	goto tr2122;
st443:
	if ( ++( p) == ( pe) )
		goto _test_eof443;
case 443:
	if ( (*( p)) == 58 )
		goto st423;
	goto tr241;
st444:
	if ( ++( p) == ( pe) )
		goto _test_eof444;
case 444:
	switch( (*( p)) ) {
		case 35: goto tr517;
		case 47: goto tr517;
		case 72: goto tr518;
		case 104: goto tr518;
	}
	goto tr241;
tr517:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st445;
st445:
	if ( ++( p) == ( pe) )
		goto _test_eof445;
case 445:
#line 10490 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 93: goto tr520;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st445;
tr518:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st446;
st446:
	if ( ++( p) == ( pe) )
		goto _test_eof446;
case 446:
#line 10507 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st447;
		case 116: goto st447;
	}
	goto tr241;
st447:
	if ( ++( p) == ( pe) )
		goto _test_eof447;
case 447:
	switch( (*( p)) ) {
		case 84: goto st448;
		case 116: goto st448;
	}
	goto tr241;
st448:
	if ( ++( p) == ( pe) )
		goto _test_eof448;
case 448:
	switch( (*( p)) ) {
		case 80: goto st449;
		case 112: goto st449;
	}
	goto tr241;
st449:
	if ( ++( p) == ( pe) )
		goto _test_eof449;
case 449:
	switch( (*( p)) ) {
		case 58: goto st450;
		case 83: goto st453;
		case 115: goto st453;
	}
	goto tr241;
st450:
	if ( ++( p) == ( pe) )
		goto _test_eof450;
case 450:
	if ( (*( p)) == 47 )
		goto st451;
	goto tr241;
st451:
	if ( ++( p) == ( pe) )
		goto _test_eof451;
case 451:
	if ( (*( p)) == 47 )
		goto st452;
	goto tr241;
st452:
	if ( ++( p) == ( pe) )
		goto _test_eof452;
case 452:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st445;
st453:
	if ( ++( p) == ( pe) )
		goto _test_eof453;
case 453:
	if ( (*( p)) == 58 )
		goto st450;
	goto tr241;
tr2080:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1680;
st1680:
	if ( ++( p) == ( pe) )
		goto _test_eof1680;
case 1680:
#line 10581 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto st454;
		case 65: goto st457;
		case 67: goto st465;
		case 71: goto st474;
		case 76: goto st480;
		case 78: goto st488;
		case 80: goto st491;
		case 81: goto st497;
		case 97: goto st457;
		case 99: goto st465;
		case 103: goto st474;
		case 108: goto st480;
		case 110: goto st488;
		case 112: goto st491;
		case 113: goto st497;
	}
	goto tr2102;
st454:
	if ( ++( p) == ( pe) )
		goto _test_eof454;
case 454:
	if ( (*( p)) == 51 )
		goto st455;
	goto tr241;
st455:
	if ( ++( p) == ( pe) )
		goto _test_eof455;
case 455:
	if ( (*( p)) == 57 )
		goto st456;
	goto tr241;
st456:
	if ( ++( p) == ( pe) )
		goto _test_eof456;
case 456:
	if ( (*( p)) == 59 )
		goto tr530;
	goto tr241;
st457:
	if ( ++( p) == ( pe) )
		goto _test_eof457;
case 457:
	switch( (*( p)) ) {
		case 77: goto st458;
		case 80: goto st460;
		case 83: goto st463;
		case 109: goto st458;
		case 112: goto st460;
		case 115: goto st463;
	}
	goto tr241;
st458:
	if ( ++( p) == ( pe) )
		goto _test_eof458;
case 458:
	switch( (*( p)) ) {
		case 80: goto st459;
		case 112: goto st459;
	}
	goto tr241;
st459:
	if ( ++( p) == ( pe) )
		goto _test_eof459;
case 459:
	if ( (*( p)) == 59 )
		goto tr535;
	goto tr241;
st460:
	if ( ++( p) == ( pe) )
		goto _test_eof460;
case 460:
	switch( (*( p)) ) {
		case 79: goto st461;
		case 111: goto st461;
	}
	goto tr241;
st461:
	if ( ++( p) == ( pe) )
		goto _test_eof461;
case 461:
	switch( (*( p)) ) {
		case 83: goto st462;
		case 115: goto st462;
	}
	goto tr241;
st462:
	if ( ++( p) == ( pe) )
		goto _test_eof462;
case 462:
	if ( (*( p)) == 59 )
		goto tr538;
	goto tr241;
st463:
	if ( ++( p) == ( pe) )
		goto _test_eof463;
case 463:
	switch( (*( p)) ) {
		case 84: goto st464;
		case 116: goto st464;
	}
	goto tr241;
st464:
	if ( ++( p) == ( pe) )
		goto _test_eof464;
case 464:
	if ( (*( p)) == 59 )
		goto tr540;
	goto tr241;
st465:
	if ( ++( p) == ( pe) )
		goto _test_eof465;
case 465:
	switch( (*( p)) ) {
		case 79: goto st466;
		case 111: goto st466;
	}
	goto tr241;
st466:
	if ( ++( p) == ( pe) )
		goto _test_eof466;
case 466:
	switch( (*( p)) ) {
		case 76: goto st467;
		case 77: goto st470;
		case 108: goto st467;
		case 109: goto st470;
	}
	goto tr241;
st467:
	if ( ++( p) == ( pe) )
		goto _test_eof467;
case 467:
	switch( (*( p)) ) {
		case 79: goto st468;
		case 111: goto st468;
	}
	goto tr241;
st468:
	if ( ++( p) == ( pe) )
		goto _test_eof468;
case 468:
	switch( (*( p)) ) {
		case 78: goto st469;
		case 110: goto st469;
	}
	goto tr241;
st469:
	if ( ++( p) == ( pe) )
		goto _test_eof469;
case 469:
	if ( (*( p)) == 59 )
		goto tr546;
	goto tr241;
st470:
	if ( ++( p) == ( pe) )
		goto _test_eof470;
case 470:
	switch( (*( p)) ) {
		case 77: goto st471;
		case 109: goto st471;
	}
	goto tr241;
st471:
	if ( ++( p) == ( pe) )
		goto _test_eof471;
case 471:
	switch( (*( p)) ) {
		case 65: goto st472;
		case 97: goto st472;
	}
	goto tr241;
st472:
	if ( ++( p) == ( pe) )
		goto _test_eof472;
case 472:
	switch( (*( p)) ) {
		case 84: goto st473;
		case 116: goto st473;
	}
	goto tr241;
st473:
	if ( ++( p) == ( pe) )
		goto _test_eof473;
case 473:
	if ( (*( p)) == 59 )
		goto tr550;
	goto tr241;
st474:
	if ( ++( p) == ( pe) )
		goto _test_eof474;
case 474:
	switch( (*( p)) ) {
		case 82: goto st475;
		case 84: goto st479;
		case 114: goto st475;
		case 116: goto st479;
	}
	goto tr241;
st475:
	if ( ++( p) == ( pe) )
		goto _test_eof475;
case 475:
	switch( (*( p)) ) {
		case 65: goto st476;
		case 97: goto st476;
	}
	goto tr241;
st476:
	if ( ++( p) == ( pe) )
		goto _test_eof476;
case 476:
	switch( (*( p)) ) {
		case 86: goto st477;
		case 118: goto st477;
	}
	goto tr241;
st477:
	if ( ++( p) == ( pe) )
		goto _test_eof477;
case 477:
	switch( (*( p)) ) {
		case 69: goto st478;
		case 101: goto st478;
	}
	goto tr241;
st478:
	if ( ++( p) == ( pe) )
		goto _test_eof478;
case 478:
	if ( (*( p)) == 59 )
		goto tr556;
	goto tr241;
st479:
	if ( ++( p) == ( pe) )
		goto _test_eof479;
case 479:
	if ( (*( p)) == 59 )
		goto tr557;
	goto tr241;
st480:
	if ( ++( p) == ( pe) )
		goto _test_eof480;
case 480:
	switch( (*( p)) ) {
		case 66: goto st481;
		case 84: goto st487;
		case 98: goto st481;
		case 116: goto st487;
	}
	goto tr241;
st481:
	if ( ++( p) == ( pe) )
		goto _test_eof481;
case 481:
	switch( (*( p)) ) {
		case 82: goto st482;
		case 114: goto st482;
	}
	goto tr241;
st482:
	if ( ++( p) == ( pe) )
		goto _test_eof482;
case 482:
	switch( (*( p)) ) {
		case 65: goto st483;
		case 97: goto st483;
	}
	goto tr241;
st483:
	if ( ++( p) == ( pe) )
		goto _test_eof483;
case 483:
	switch( (*( p)) ) {
		case 67: goto st484;
		case 99: goto st484;
	}
	goto tr241;
st484:
	if ( ++( p) == ( pe) )
		goto _test_eof484;
case 484:
	switch( (*( p)) ) {
		case 69: goto st485;
		case 75: goto st486;
		case 101: goto st485;
		case 107: goto st486;
	}
	goto tr241;
st485:
	if ( ++( p) == ( pe) )
		goto _test_eof485;
case 485:
	if ( (*( p)) == 59 )
		goto tr565;
	goto tr241;
st486:
	if ( ++( p) == ( pe) )
		goto _test_eof486;
case 486:
	if ( (*( p)) == 59 )
		goto tr566;
	goto tr241;
st487:
	if ( ++( p) == ( pe) )
		goto _test_eof487;
case 487:
	if ( (*( p)) == 59 )
		goto tr567;
	goto tr241;
st488:
	if ( ++( p) == ( pe) )
		goto _test_eof488;
case 488:
	switch( (*( p)) ) {
		case 85: goto st489;
		case 117: goto st489;
	}
	goto tr241;
st489:
	if ( ++( p) == ( pe) )
		goto _test_eof489;
case 489:
	switch( (*( p)) ) {
		case 77: goto st490;
		case 109: goto st490;
	}
	goto tr241;
st490:
	if ( ++( p) == ( pe) )
		goto _test_eof490;
case 490:
	if ( (*( p)) == 59 )
		goto tr570;
	goto tr241;
st491:
	if ( ++( p) == ( pe) )
		goto _test_eof491;
case 491:
	switch( (*( p)) ) {
		case 69: goto st492;
		case 101: goto st492;
	}
	goto tr241;
st492:
	if ( ++( p) == ( pe) )
		goto _test_eof492;
case 492:
	switch( (*( p)) ) {
		case 82: goto st493;
		case 114: goto st493;
	}
	goto tr241;
st493:
	if ( ++( p) == ( pe) )
		goto _test_eof493;
case 493:
	switch( (*( p)) ) {
		case 73: goto st494;
		case 105: goto st494;
	}
	goto tr241;
st494:
	if ( ++( p) == ( pe) )
		goto _test_eof494;
case 494:
	switch( (*( p)) ) {
		case 79: goto st495;
		case 111: goto st495;
	}
	goto tr241;
st495:
	if ( ++( p) == ( pe) )
		goto _test_eof495;
case 495:
	switch( (*( p)) ) {
		case 68: goto st496;
		case 100: goto st496;
	}
	goto tr241;
st496:
	if ( ++( p) == ( pe) )
		goto _test_eof496;
case 496:
	if ( (*( p)) == 59 )
		goto tr576;
	goto tr241;
st497:
	if ( ++( p) == ( pe) )
		goto _test_eof497;
case 497:
	switch( (*( p)) ) {
		case 85: goto st498;
		case 117: goto st498;
	}
	goto tr241;
st498:
	if ( ++( p) == ( pe) )
		goto _test_eof498;
case 498:
	switch( (*( p)) ) {
		case 79: goto st499;
		case 111: goto st499;
	}
	goto tr241;
st499:
	if ( ++( p) == ( pe) )
		goto _test_eof499;
case 499:
	switch( (*( p)) ) {
		case 84: goto st500;
		case 116: goto st500;
	}
	goto tr241;
st500:
	if ( ++( p) == ( pe) )
		goto _test_eof500;
case 500:
	if ( (*( p)) == 59 )
		goto tr580;
	goto tr241;
tr2131:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1681;
tr2081:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1681;
st1681:
	if ( ++( p) == ( pe) )
		goto _test_eof1681;
case 1681:
#line 11021 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2132:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st501;
st501:
	if ( ++( p) == ( pe) )
		goto _test_eof501;
case 501:
#line 11043 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 91 )
		goto st502;
	goto tr237;
tr583:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st502;
st502:
	if ( ++( p) == ( pe) )
		goto _test_eof502;
case 502:
#line 11055 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr583;
		case 32: goto tr583;
		case 58: goto tr585;
		case 60: goto tr586;
		case 62: goto tr587;
		case 92: goto tr588;
		case 93: goto tr237;
		case 124: goto tr589;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr584;
	goto tr582;
tr582:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st503;
st503:
	if ( ++( p) == ( pe) )
		goto _test_eof503;
case 503:
#line 11077 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr591;
		case 32: goto tr591;
		case 35: goto tr593;
		case 93: goto tr594;
		case 124: goto tr595;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st505;
	goto st503;
tr591:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st504;
st504:
	if ( ++( p) == ( pe) )
		goto _test_eof504;
case 504:
#line 11096 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st504;
		case 32: goto st504;
		case 35: goto st506;
		case 93: goto st509;
		case 124: goto st510;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st505;
	goto st503;
tr584:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st505;
st505:
	if ( ++( p) == ( pe) )
		goto _test_eof505;
case 505:
#line 11115 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st505;
		case 93: goto tr234;
		case 124: goto tr234;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st505;
	goto st503;
tr593:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st506;
st506:
	if ( ++( p) == ( pe) )
		goto _test_eof506;
case 506:
#line 11132 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr591;
		case 32: goto tr591;
		case 35: goto tr593;
		case 93: goto tr594;
		case 124: goto tr595;
	}
	if ( (*( p)) > 13 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr600;
	} else if ( (*( p)) >= 10 )
		goto st505;
	goto st503;
tr600:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st507;
st507:
	if ( ++( p) == ( pe) )
		goto _test_eof507;
case 507:
#line 11154 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr601;
		case 32: goto tr602;
		case 45: goto st515;
		case 93: goto tr605;
		case 95: goto st515;
		case 124: goto tr606;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st507;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st507;
	} else
		goto st507;
	goto tr234;
tr601:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st508;
st508:
	if ( ++( p) == ( pe) )
		goto _test_eof508;
case 508:
#line 11180 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st508;
		case 32: goto st508;
		case 93: goto st509;
		case 124: goto st510;
	}
	goto tr234;
tr594:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st509;
tr605:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st509;
st509:
	if ( ++( p) == ( pe) )
		goto _test_eof509;
case 509:
#line 11200 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1682;
	goto tr234;
st1682:
	if ( ++( p) == ( pe) )
		goto _test_eof1682;
case 1682:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2135;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2135;
	} else
		goto tr2135;
	goto tr2134;
tr2135:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1683;
st1683:
	if ( ++( p) == ( pe) )
		goto _test_eof1683;
case 1683:
#line 11225 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1683;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1683;
	} else
		goto st1683;
	goto tr2136;
tr595:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st510;
tr606:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st510;
tr610:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st510;
st510:
	if ( ++( p) == ( pe) )
		goto _test_eof510;
case 510:
#line 11253 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr610;
		case 32: goto tr610;
		case 93: goto tr611;
		case 124: goto tr234;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto tr609;
tr609:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st511;
st511:
	if ( ++( p) == ( pe) )
		goto _test_eof511;
case 511:
#line 11271 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr613;
		case 32: goto tr613;
		case 93: goto tr614;
		case 124: goto tr234;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st511;
tr613:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st512;
st512:
	if ( ++( p) == ( pe) )
		goto _test_eof512;
case 512:
#line 11289 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st512;
		case 32: goto st512;
		case 93: goto st513;
		case 124: goto tr234;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st511;
tr611:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st513;
tr614:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st513;
st513:
	if ( ++( p) == ( pe) )
		goto _test_eof513;
case 513:
#line 11313 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1684;
	goto tr234;
st1684:
	if ( ++( p) == ( pe) )
		goto _test_eof1684;
case 1684:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2139;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2139;
	} else
		goto tr2139;
	goto tr2138;
tr2139:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1685;
st1685:
	if ( ++( p) == ( pe) )
		goto _test_eof1685;
case 1685:
#line 11338 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1685;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1685;
	} else
		goto st1685;
	goto tr2140;
tr602:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st514;
st514:
	if ( ++( p) == ( pe) )
		goto _test_eof514;
case 514:
#line 11356 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st508;
		case 32: goto st514;
		case 45: goto st515;
		case 93: goto st509;
		case 95: goto st515;
		case 124: goto st510;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st507;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st507;
	} else
		goto st507;
	goto tr234;
st515:
	if ( ++( p) == ( pe) )
		goto _test_eof515;
case 515:
	switch( (*( p)) ) {
		case 32: goto st515;
		case 45: goto st515;
		case 95: goto st515;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st507;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st507;
	} else
		goto st507;
	goto tr234;
tr585:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st516;
st516:
	if ( ++( p) == ( pe) )
		goto _test_eof516;
case 516:
#line 11400 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr591;
		case 32: goto tr591;
		case 35: goto tr593;
		case 93: goto tr594;
		case 124: goto tr619;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st505;
	goto st503;
tr619:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st517;
st517:
	if ( ++( p) == ( pe) )
		goto _test_eof517;
case 517:
#line 11419 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr620;
		case 32: goto tr620;
		case 35: goto tr621;
		case 93: goto tr622;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto tr609;
tr623:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st518;
tr620:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st518;
st518:
	if ( ++( p) == ( pe) )
		goto _test_eof518;
case 518:
#line 11448 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr623;
		case 32: goto tr623;
		case 35: goto tr624;
		case 93: goto tr625;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto tr609;
tr659:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st519;
tr624:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st519;
tr621:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st519;
st519:
	if ( ++( p) == ( pe) )
		goto _test_eof519;
case 519:
#line 11477 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr613;
		case 32: goto tr613;
		case 93: goto tr614;
		case 124: goto tr237;
	}
	if ( (*( p)) > 13 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr626;
	} else if ( (*( p)) >= 10 )
		goto tr237;
	goto st511;
tr626:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st520;
st520:
	if ( ++( p) == ( pe) )
		goto _test_eof520;
case 520:
#line 11498 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr627;
		case 32: goto tr628;
		case 45: goto st524;
		case 93: goto tr631;
		case 95: goto st524;
		case 124: goto tr237;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr237;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st520;
		} else if ( (*( p)) >= 65 )
			goto st520;
	} else
		goto st520;
	goto st511;
tr627:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st521;
st521:
	if ( ++( p) == ( pe) )
		goto _test_eof521;
case 521:
#line 11529 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st521;
		case 32: goto st521;
		case 93: goto st522;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto st511;
tr625:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st522;
tr622:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st522;
tr631:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st522;
tr660:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st522;
st522:
	if ( ++( p) == ( pe) )
		goto _test_eof522;
case 522:
#line 11569 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1686;
	goto tr237;
st1686:
	if ( ++( p) == ( pe) )
		goto _test_eof1686;
case 1686:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2142;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2142;
	} else
		goto tr2142;
	goto tr2134;
tr2142:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1687;
st1687:
	if ( ++( p) == ( pe) )
		goto _test_eof1687;
case 1687:
#line 11594 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1687;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1687;
	} else
		goto st1687;
	goto tr2136;
tr628:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st523;
st523:
	if ( ++( p) == ( pe) )
		goto _test_eof523;
case 523:
#line 11614 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st521;
		case 32: goto st523;
		case 45: goto st524;
		case 93: goto st522;
		case 95: goto st524;
		case 124: goto tr237;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr237;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st520;
		} else if ( (*( p)) >= 65 )
			goto st520;
	} else
		goto st520;
	goto st511;
st524:
	if ( ++( p) == ( pe) )
		goto _test_eof524;
case 524:
	switch( (*( p)) ) {
		case 9: goto tr613;
		case 32: goto tr636;
		case 45: goto st524;
		case 93: goto tr614;
		case 95: goto st524;
		case 124: goto tr237;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr237;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st520;
		} else if ( (*( p)) >= 65 )
			goto st520;
	} else
		goto st520;
	goto st511;
tr636:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st525;
st525:
	if ( ++( p) == ( pe) )
		goto _test_eof525;
case 525:
#line 11667 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st512;
		case 32: goto st525;
		case 45: goto st524;
		case 93: goto st513;
		case 95: goto st524;
		case 124: goto tr237;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr237;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st520;
		} else if ( (*( p)) >= 65 )
			goto st520;
	} else
		goto st520;
	goto st511;
tr586:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st526;
st526:
	if ( ++( p) == ( pe) )
		goto _test_eof526;
case 526:
#line 11696 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr591;
		case 32: goto tr591;
		case 35: goto tr593;
		case 93: goto tr594;
		case 124: goto tr638;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st505;
	goto st503;
tr638:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st527;
st527:
	if ( ++( p) == ( pe) )
		goto _test_eof527;
case 527:
#line 11715 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr610;
		case 32: goto tr610;
		case 62: goto tr639;
		case 93: goto tr611;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto tr609;
tr639:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st528;
st528:
	if ( ++( p) == ( pe) )
		goto _test_eof528;
case 528:
#line 11734 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr613;
		case 32: goto tr613;
		case 93: goto tr614;
		case 95: goto st529;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto st511;
st529:
	if ( ++( p) == ( pe) )
		goto _test_eof529;
case 529:
	switch( (*( p)) ) {
		case 9: goto tr613;
		case 32: goto tr613;
		case 60: goto st530;
		case 93: goto tr614;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto st511;
st530:
	if ( ++( p) == ( pe) )
		goto _test_eof530;
case 530:
	switch( (*( p)) ) {
		case 9: goto tr613;
		case 32: goto tr613;
		case 93: goto tr614;
		case 124: goto st531;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto st511;
st531:
	if ( ++( p) == ( pe) )
		goto _test_eof531;
case 531:
	if ( (*( p)) == 62 )
		goto st532;
	goto tr237;
st532:
	if ( ++( p) == ( pe) )
		goto _test_eof532;
case 532:
	switch( (*( p)) ) {
		case 9: goto tr644;
		case 32: goto tr644;
		case 35: goto tr645;
		case 93: goto tr594;
	}
	goto tr237;
tr644:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st533;
st533:
	if ( ++( p) == ( pe) )
		goto _test_eof533;
case 533:
#line 11798 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st533;
		case 32: goto st533;
		case 35: goto st534;
		case 93: goto st509;
	}
	goto tr237;
tr645:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st534;
st534:
	if ( ++( p) == ( pe) )
		goto _test_eof534;
case 534:
#line 11814 "ext/dtext/dtext.cpp"
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr648;
	goto tr237;
tr648:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st535;
st535:
	if ( ++( p) == ( pe) )
		goto _test_eof535;
case 535:
#line 11826 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr649;
		case 32: goto tr650;
		case 45: goto st538;
		case 93: goto tr605;
		case 95: goto st538;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st535;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st535;
	} else
		goto st535;
	goto tr237;
tr649:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st536;
st536:
	if ( ++( p) == ( pe) )
		goto _test_eof536;
case 536:
#line 11851 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st536;
		case 32: goto st536;
		case 93: goto st509;
	}
	goto tr237;
tr650:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st537;
st537:
	if ( ++( p) == ( pe) )
		goto _test_eof537;
case 537:
#line 11866 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st536;
		case 32: goto st537;
		case 45: goto st538;
		case 93: goto st509;
		case 95: goto st538;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st535;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st535;
	} else
		goto st535;
	goto tr237;
st538:
	if ( ++( p) == ( pe) )
		goto _test_eof538;
case 538:
	switch( (*( p)) ) {
		case 32: goto st538;
		case 45: goto st538;
		case 95: goto st538;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st535;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st535;
	} else
		goto st535;
	goto tr237;
tr587:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st539;
st539:
	if ( ++( p) == ( pe) )
		goto _test_eof539;
case 539:
#line 11909 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr591;
		case 32: goto tr591;
		case 35: goto tr593;
		case 58: goto st516;
		case 93: goto tr594;
		case 124: goto tr656;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st505;
	goto st503;
tr656:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st540;
st540:
	if ( ++( p) == ( pe) )
		goto _test_eof540;
case 540:
#line 11929 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr610;
		case 32: goto tr610;
		case 51: goto tr657;
		case 93: goto tr611;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto tr609;
tr657:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st541;
st541:
	if ( ++( p) == ( pe) )
		goto _test_eof541;
case 541:
#line 11948 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr658;
		case 32: goto tr658;
		case 35: goto tr659;
		case 93: goto tr660;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto st511;
tr658:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st542;
st542:
	if ( ++( p) == ( pe) )
		goto _test_eof542;
case 542:
#line 11969 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st542;
		case 32: goto st542;
		case 35: goto st519;
		case 93: goto st522;
		case 124: goto tr237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto st511;
tr588:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st543;
st543:
	if ( ++( p) == ( pe) )
		goto _test_eof543;
case 543:
#line 11988 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr591;
		case 32: goto tr591;
		case 35: goto tr593;
		case 93: goto tr594;
		case 124: goto tr663;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st505;
	goto st503;
tr663:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st544;
st544:
	if ( ++( p) == ( pe) )
		goto _test_eof544;
case 544:
#line 12007 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr610;
		case 32: goto tr610;
		case 93: goto tr611;
		case 124: goto st545;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr237;
	goto tr609;
st545:
	if ( ++( p) == ( pe) )
		goto _test_eof545;
case 545:
	if ( (*( p)) == 47 )
		goto st532;
	goto tr237;
tr589:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st546;
st546:
	if ( ++( p) == ( pe) )
		goto _test_eof546;
case 546:
#line 12032 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 95: goto st550;
		case 119: goto st551;
		case 124: goto st552;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st547;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st547;
	} else
		goto st547;
	goto tr237;
st547:
	if ( ++( p) == ( pe) )
		goto _test_eof547;
case 547:
	switch( (*( p)) ) {
		case 9: goto tr669;
		case 32: goto tr669;
		case 35: goto tr670;
		case 93: goto tr594;
		case 124: goto tr595;
	}
	goto tr237;
tr669:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st548;
st548:
	if ( ++( p) == ( pe) )
		goto _test_eof548;
case 548:
#line 12067 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st548;
		case 32: goto st548;
		case 35: goto st549;
		case 93: goto st509;
		case 124: goto st510;
	}
	goto tr237;
tr670:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st549;
st549:
	if ( ++( p) == ( pe) )
		goto _test_eof549;
case 549:
#line 12084 "ext/dtext/dtext.cpp"
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr600;
	goto tr237;
st550:
	if ( ++( p) == ( pe) )
		goto _test_eof550;
case 550:
	if ( (*( p)) == 124 )
		goto st547;
	goto tr237;
st551:
	if ( ++( p) == ( pe) )
		goto _test_eof551;
case 551:
	switch( (*( p)) ) {
		case 9: goto tr669;
		case 32: goto tr669;
		case 35: goto tr670;
		case 93: goto tr594;
		case 124: goto tr619;
	}
	goto tr237;
st552:
	if ( ++( p) == ( pe) )
		goto _test_eof552;
case 552:
	if ( (*( p)) == 95 )
		goto st553;
	goto tr237;
st553:
	if ( ++( p) == ( pe) )
		goto _test_eof553;
case 553:
	if ( (*( p)) == 124 )
		goto st550;
	goto tr237;
tr2133:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st554;
st554:
	if ( ++( p) == ( pe) )
		goto _test_eof554;
case 554:
#line 12129 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 123 )
		goto st555;
	goto tr237;
st555:
	if ( ++( p) == ( pe) )
		goto _test_eof555;
case 555:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto st555;
		case 32: goto st555;
		case 45: goto tr676;
		case 58: goto tr677;
		case 60: goto tr678;
		case 62: goto tr679;
		case 92: goto tr680;
		case 124: goto tr681;
		case 126: goto tr676;
	}
	if ( (*( p)) > 13 ) {
		if ( 123 <= (*( p)) && (*( p)) <= 125 )
			goto tr234;
	} else if ( (*( p)) >= 10 )
		goto tr234;
	goto tr675;
tr675:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st556;
st556:
	if ( ++( p) == ( pe) )
		goto _test_eof556;
case 556:
#line 12163 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr683;
		case 32: goto tr683;
		case 123: goto tr234;
		case 124: goto tr684;
		case 125: goto tr685;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st556;
tr683:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st557;
st557:
	if ( ++( p) == ( pe) )
		goto _test_eof557;
case 557:
#line 12183 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto st557;
		case 32: goto st557;
		case 45: goto st558;
		case 58: goto st559;
		case 60: goto st594;
		case 62: goto st595;
		case 92: goto st597;
		case 123: goto tr234;
		case 124: goto st588;
		case 125: goto st566;
		case 126: goto st558;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st556;
tr676:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st558;
st558:
	if ( ++( p) == ( pe) )
		goto _test_eof558;
case 558:
#line 12209 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr683;
		case 32: goto tr683;
		case 58: goto st559;
		case 60: goto st594;
		case 62: goto st595;
		case 92: goto st597;
		case 123: goto tr234;
		case 124: goto tr694;
		case 125: goto tr685;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st556;
tr677:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st559;
st559:
	if ( ++( p) == ( pe) )
		goto _test_eof559;
case 559:
#line 12233 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr683;
		case 32: goto tr683;
		case 123: goto st560;
		case 124: goto tr696;
		case 125: goto tr697;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st556;
st560:
	if ( ++( p) == ( pe) )
		goto _test_eof560;
case 560:
	switch( (*( p)) ) {
		case 9: goto tr683;
		case 32: goto tr683;
		case 124: goto tr684;
		case 125: goto tr685;
	}
	goto tr234;
tr684:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st561;
tr699:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st561;
tr711:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st561;
st561:
	if ( ++( p) == ( pe) )
		goto _test_eof561;
case 561:
#line 12276 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr699;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr699;
		case 125: goto tr701;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr700;
	goto tr698;
tr698:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st562;
st562:
	if ( ++( p) == ( pe) )
		goto _test_eof562;
case 562:
#line 12296 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr703:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st563;
st563:
	if ( ++( p) == ( pe) )
		goto _test_eof563;
case 563:
#line 12316 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto st563;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto st563;
		case 125: goto st565;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr700:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st564;
st564:
	if ( ++( p) == ( pe) )
		goto _test_eof564;
case 564:
#line 12336 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto st564;
		case 125: goto tr234;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr705:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st565;
tr701:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st565;
st565:
	if ( ++( p) == ( pe) )
		goto _test_eof565;
case 565:
#line 12361 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1688;
	goto tr234;
st1688:
	if ( ++( p) == ( pe) )
		goto _test_eof1688;
case 1688:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2145;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2145;
	} else
		goto tr2145;
	goto tr2144;
tr2145:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1689;
st1689:
	if ( ++( p) == ( pe) )
		goto _test_eof1689;
case 1689:
#line 12386 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1689;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1689;
	} else
		goto st1689;
	goto tr2146;
tr685:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st566;
st566:
	if ( ++( p) == ( pe) )
		goto _test_eof566;
case 566:
#line 12404 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1690;
	goto tr234;
tr2154:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1690;
st1690:
	if ( ++( p) == ( pe) )
		goto _test_eof1690;
case 1690:
#line 12418 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2149;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2149;
	} else
		goto tr2149;
	goto tr2148;
tr2149:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1691;
st1691:
	if ( ++( p) == ( pe) )
		goto _test_eof1691;
case 1691:
#line 12436 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1691;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1691;
	} else
		goto st1691;
	goto tr2150;
tr696:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st567;
st567:
	if ( ++( p) == ( pe) )
		goto _test_eof567;
case 567:
#line 12454 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr710;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr710;
		case 124: goto tr711;
		case 125: goto tr712;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr700;
	goto tr698;
tr714:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st568;
tr710:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st568;
st568:
	if ( ++( p) == ( pe) )
		goto _test_eof568;
case 568:
#line 12485 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr714;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr714;
		case 45: goto tr715;
		case 58: goto tr716;
		case 60: goto tr717;
		case 62: goto tr718;
		case 92: goto tr719;
		case 123: goto tr698;
		case 124: goto tr720;
		case 125: goto tr721;
		case 126: goto tr715;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr700;
	goto tr713;
tr713:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st569;
st569:
	if ( ++( p) == ( pe) )
		goto _test_eof569;
case 569:
#line 12513 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 123: goto st562;
		case 124: goto tr684;
		case 125: goto tr724;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st569;
tr723:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st570;
st570:
	if ( ++( p) == ( pe) )
		goto _test_eof570;
case 570:
#line 12537 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto st570;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto st570;
		case 45: goto st571;
		case 58: goto st572;
		case 60: goto st576;
		case 62: goto st582;
		case 92: goto st585;
		case 123: goto st562;
		case 124: goto st588;
		case 125: goto st574;
		case 126: goto st571;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st569;
tr715:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st571;
st571:
	if ( ++( p) == ( pe) )
		goto _test_eof571;
case 571:
#line 12565 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 58: goto st572;
		case 60: goto st576;
		case 62: goto st582;
		case 92: goto st585;
		case 123: goto st562;
		case 124: goto tr694;
		case 125: goto tr724;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st569;
tr716:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st572;
st572:
	if ( ++( p) == ( pe) )
		goto _test_eof572;
case 572:
#line 12591 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 123: goto st573;
		case 124: goto tr696;
		case 125: goto tr733;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st569;
tr743:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st573;
st573:
	if ( ++( p) == ( pe) )
		goto _test_eof573;
case 573:
#line 12613 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 124: goto tr684;
		case 125: goto tr724;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr721:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st574;
tr712:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st574;
tr724:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st574;
st574:
	if ( ++( p) == ( pe) )
		goto _test_eof574;
case 574:
#line 12650 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1692;
	goto tr234;
st1692:
	if ( ++( p) == ( pe) )
		goto _test_eof1692;
case 1692:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2152;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2152;
	} else
		goto tr2152;
	goto tr2148;
tr2152:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1693;
st1693:
	if ( ++( p) == ( pe) )
		goto _test_eof1693;
case 1693:
#line 12675 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1693;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1693;
	} else
		goto st1693;
	goto tr2150;
tr733:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st575;
st575:
	if ( ++( p) == ( pe) )
		goto _test_eof575;
case 575:
#line 12695 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr683;
		case 32: goto tr683;
		case 124: goto tr684;
		case 125: goto tr735;
	}
	goto tr234;
tr735:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1694;
st1694:
	if ( ++( p) == ( pe) )
		goto _test_eof1694;
case 1694:
#line 12711 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto tr2154;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2152;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2152;
	} else
		goto tr2152;
	goto tr2148;
tr717:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st576;
st576:
	if ( ++( p) == ( pe) )
		goto _test_eof576;
case 576:
#line 12731 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 123: goto st562;
		case 124: goto tr736;
		case 125: goto tr724;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st569;
tr736:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st577;
st577:
	if ( ++( p) == ( pe) )
		goto _test_eof577;
case 577:
#line 12753 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr699;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr699;
		case 62: goto tr737;
		case 125: goto tr701;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr700;
	goto tr698;
tr737:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st578;
st578:
	if ( ++( p) == ( pe) )
		goto _test_eof578;
case 578:
#line 12774 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 95: goto st579;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
st579:
	if ( ++( p) == ( pe) )
		goto _test_eof579;
case 579:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 60: goto st580;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
st580:
	if ( ++( p) == ( pe) )
		goto _test_eof580;
case 580:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 124: goto st581;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
st581:
	if ( ++( p) == ( pe) )
		goto _test_eof581;
case 581:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 62: goto st573;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr718:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st582;
st582:
	if ( ++( p) == ( pe) )
		goto _test_eof582;
case 582:
#line 12843 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 58: goto st583;
		case 123: goto st562;
		case 124: goto tr742;
		case 125: goto tr724;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st569;
st583:
	if ( ++( p) == ( pe) )
		goto _test_eof583;
case 583:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 123: goto st562;
		case 124: goto tr696;
		case 125: goto tr724;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st569;
tr742:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st584;
st584:
	if ( ++( p) == ( pe) )
		goto _test_eof584;
case 584:
#line 12883 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr699;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr699;
		case 51: goto tr743;
		case 125: goto tr701;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr700;
	goto tr698;
tr719:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st585;
st585:
	if ( ++( p) == ( pe) )
		goto _test_eof585;
case 585:
#line 12904 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 123: goto st562;
		case 124: goto tr744;
		case 125: goto tr724;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st569;
tr744:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st586;
st586:
	if ( ++( p) == ( pe) )
		goto _test_eof586;
case 586:
#line 12926 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr699;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr699;
		case 124: goto tr745;
		case 125: goto tr701;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr700;
	goto tr698;
tr745:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st587;
st587:
	if ( ++( p) == ( pe) )
		goto _test_eof587;
case 587:
#line 12947 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 47: goto st573;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr694:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st588;
tr720:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st588;
st588:
	if ( ++( p) == ( pe) )
		goto _test_eof588;
case 588:
#line 12972 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr699;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr699;
		case 95: goto tr746;
		case 119: goto tr747;
		case 124: goto tr748;
		case 125: goto tr701;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto tr700;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr743;
		} else if ( (*( p)) >= 65 )
			goto tr743;
	} else
		goto tr743;
	goto tr698;
tr746:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st589;
st589:
	if ( ++( p) == ( pe) )
		goto _test_eof589;
case 589:
#line 13004 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 124: goto st573;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr747:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st590;
st590:
	if ( ++( p) == ( pe) )
		goto _test_eof590;
case 590:
#line 13025 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr723;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr723;
		case 124: goto tr696;
		case 125: goto tr724;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr748:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st591;
st591:
	if ( ++( p) == ( pe) )
		goto _test_eof591;
case 591:
#line 13046 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 95: goto st592;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
st592:
	if ( ++( p) == ( pe) )
		goto _test_eof592;
case 592:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr703;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr703;
		case 124: goto st589;
		case 125: goto tr705;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st564;
	goto st562;
tr697:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st593;
st593:
	if ( ++( p) == ( pe) )
		goto _test_eof593;
case 593:
#line 13083 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr683;
		case 32: goto tr683;
		case 124: goto tr684;
		case 125: goto tr751;
	}
	goto tr234;
tr751:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1695;
st1695:
	if ( ++( p) == ( pe) )
		goto _test_eof1695;
case 1695:
#line 13099 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1690;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2149;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2149;
	} else
		goto tr2149;
	goto tr2148;
tr678:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st594;
st594:
	if ( ++( p) == ( pe) )
		goto _test_eof594;
case 594:
#line 13119 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr683;
		case 32: goto tr683;
		case 123: goto tr234;
		case 124: goto tr736;
		case 125: goto tr685;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st556;
tr679:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st595;
st595:
	if ( ++( p) == ( pe) )
		goto _test_eof595;
case 595:
#line 13139 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr683;
		case 32: goto tr683;
		case 58: goto st596;
		case 123: goto tr234;
		case 124: goto tr742;
		case 125: goto tr685;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st556;
st596:
	if ( ++( p) == ( pe) )
		goto _test_eof596;
case 596:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr683;
		case 32: goto tr683;
		case 123: goto tr234;
		case 124: goto tr696;
		case 125: goto tr685;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st556;
tr680:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st597;
st597:
	if ( ++( p) == ( pe) )
		goto _test_eof597;
case 597:
#line 13175 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr683;
		case 32: goto tr683;
		case 123: goto tr234;
		case 124: goto tr744;
		case 125: goto tr685;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st556;
tr681:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st598;
st598:
	if ( ++( p) == ( pe) )
		goto _test_eof598;
case 598:
#line 13195 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 95: goto st599;
		case 119: goto st600;
		case 124: goto st601;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st560;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st560;
	} else
		goto st560;
	goto tr234;
st599:
	if ( ++( p) == ( pe) )
		goto _test_eof599;
case 599:
	if ( (*( p)) == 124 )
		goto st560;
	goto tr234;
st600:
	if ( ++( p) == ( pe) )
		goto _test_eof600;
case 600:
	switch( (*( p)) ) {
		case 9: goto tr683;
		case 32: goto tr683;
		case 124: goto tr696;
		case 125: goto tr685;
	}
	goto tr234;
st601:
	if ( ++( p) == ( pe) )
		goto _test_eof601;
case 601:
	if ( (*( p)) == 95 )
		goto st602;
	goto tr234;
st602:
	if ( ++( p) == ( pe) )
		goto _test_eof602;
case 602:
	if ( (*( p)) == 124 )
		goto st599;
	goto tr234;
tr2082:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1696;
st1696:
	if ( ++( p) == ( pe) )
		goto _test_eof1696;
case 1696:
#line 13254 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2155;
		case 82: goto tr2156;
		case 86: goto tr2157;
		case 91: goto tr2132;
		case 108: goto tr2155;
		case 114: goto tr2156;
		case 118: goto tr2157;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2155:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1697;
st1697:
	if ( ++( p) == ( pe) )
		goto _test_eof1697;
case 1697:
#line 13284 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2158;
		case 91: goto tr2132;
		case 105: goto tr2158;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2158:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1698;
st1698:
	if ( ++( p) == ( pe) )
		goto _test_eof1698;
case 1698:
#line 13310 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2159;
		case 91: goto tr2132;
		case 97: goto tr2159;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2159:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1699;
st1699:
	if ( ++( p) == ( pe) )
		goto _test_eof1699;
case 1699:
#line 13336 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2160;
		case 91: goto tr2132;
		case 115: goto tr2160;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2160:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1700;
st1700:
	if ( ++( p) == ( pe) )
		goto _test_eof1700;
case 1700:
#line 13362 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st603;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st603:
	if ( ++( p) == ( pe) )
		goto _test_eof603;
case 603:
	if ( (*( p)) == 35 )
		goto st604;
	goto tr237;
st604:
	if ( ++( p) == ( pe) )
		goto _test_eof604;
case 604:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr758;
	goto tr237;
tr758:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1701;
st1701:
	if ( ++( p) == ( pe) )
		goto _test_eof1701;
case 1701:
#line 13399 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1701;
	goto tr2162;
tr2156:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1702;
st1702:
	if ( ++( p) == ( pe) )
		goto _test_eof1702;
case 1702:
#line 13413 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2164;
		case 91: goto tr2132;
		case 116: goto tr2164;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2164:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1703;
st1703:
	if ( ++( p) == ( pe) )
		goto _test_eof1703;
case 1703:
#line 13439 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2165;
		case 91: goto tr2132;
		case 105: goto tr2165;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2165:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1704;
st1704:
	if ( ++( p) == ( pe) )
		goto _test_eof1704;
case 1704:
#line 13465 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2166;
		case 91: goto tr2132;
		case 115: goto tr2166;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2166:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1705;
st1705:
	if ( ++( p) == ( pe) )
		goto _test_eof1705;
case 1705:
#line 13491 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2167;
		case 91: goto tr2132;
		case 116: goto tr2167;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2167:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1706;
st1706:
	if ( ++( p) == ( pe) )
		goto _test_eof1706;
case 1706:
#line 13517 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st605;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st605:
	if ( ++( p) == ( pe) )
		goto _test_eof605;
case 605:
	switch( (*( p)) ) {
		case 35: goto st606;
		case 67: goto st607;
		case 99: goto st607;
	}
	goto tr237;
st606:
	if ( ++( p) == ( pe) )
		goto _test_eof606;
case 606:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr761;
	goto tr237;
tr761:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1707;
st1707:
	if ( ++( p) == ( pe) )
		goto _test_eof1707;
case 1707:
#line 13557 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1707;
	goto tr2169;
st607:
	if ( ++( p) == ( pe) )
		goto _test_eof607;
case 607:
	switch( (*( p)) ) {
		case 72: goto st608;
		case 104: goto st608;
	}
	goto tr237;
st608:
	if ( ++( p) == ( pe) )
		goto _test_eof608;
case 608:
	switch( (*( p)) ) {
		case 65: goto st609;
		case 97: goto st609;
	}
	goto tr237;
st609:
	if ( ++( p) == ( pe) )
		goto _test_eof609;
case 609:
	switch( (*( p)) ) {
		case 78: goto st610;
		case 110: goto st610;
	}
	goto tr237;
st610:
	if ( ++( p) == ( pe) )
		goto _test_eof610;
case 610:
	switch( (*( p)) ) {
		case 71: goto st611;
		case 103: goto st611;
	}
	goto tr237;
st611:
	if ( ++( p) == ( pe) )
		goto _test_eof611;
case 611:
	switch( (*( p)) ) {
		case 69: goto st612;
		case 101: goto st612;
	}
	goto tr237;
st612:
	if ( ++( p) == ( pe) )
		goto _test_eof612;
case 612:
	switch( (*( p)) ) {
		case 83: goto st613;
		case 115: goto st613;
	}
	goto tr237;
st613:
	if ( ++( p) == ( pe) )
		goto _test_eof613;
case 613:
	if ( (*( p)) == 32 )
		goto st614;
	goto tr237;
st614:
	if ( ++( p) == ( pe) )
		goto _test_eof614;
case 614:
	if ( (*( p)) == 35 )
		goto st615;
	goto tr237;
st615:
	if ( ++( p) == ( pe) )
		goto _test_eof615;
case 615:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr770;
	goto tr237;
tr770:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1708;
st1708:
	if ( ++( p) == ( pe) )
		goto _test_eof1708;
case 1708:
#line 13644 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1708;
	goto tr2171;
tr2157:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1709;
st1709:
	if ( ++( p) == ( pe) )
		goto _test_eof1709;
case 1709:
#line 13658 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2173;
		case 91: goto tr2132;
		case 111: goto tr2173;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2173:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1710;
st1710:
	if ( ++( p) == ( pe) )
		goto _test_eof1710;
case 1710:
#line 13684 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2174;
		case 91: goto tr2132;
		case 105: goto tr2174;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2174:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1711;
st1711:
	if ( ++( p) == ( pe) )
		goto _test_eof1711;
case 1711:
#line 13710 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2175;
		case 91: goto tr2132;
		case 100: goto tr2175;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2175:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1712;
st1712:
	if ( ++( p) == ( pe) )
		goto _test_eof1712;
case 1712:
#line 13736 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st616;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st616:
	if ( ++( p) == ( pe) )
		goto _test_eof616;
case 616:
	switch( (*( p)) ) {
		case 80: goto st617;
		case 112: goto st617;
	}
	goto tr237;
st617:
	if ( ++( p) == ( pe) )
		goto _test_eof617;
case 617:
	switch( (*( p)) ) {
		case 79: goto st618;
		case 111: goto st618;
	}
	goto tr237;
st618:
	if ( ++( p) == ( pe) )
		goto _test_eof618;
case 618:
	switch( (*( p)) ) {
		case 83: goto st619;
		case 115: goto st619;
	}
	goto tr237;
st619:
	if ( ++( p) == ( pe) )
		goto _test_eof619;
case 619:
	switch( (*( p)) ) {
		case 84: goto st620;
		case 116: goto st620;
	}
	goto tr237;
st620:
	if ( ++( p) == ( pe) )
		goto _test_eof620;
case 620:
	switch( (*( p)) ) {
		case 73: goto st621;
		case 105: goto st621;
	}
	goto tr237;
st621:
	if ( ++( p) == ( pe) )
		goto _test_eof621;
case 621:
	switch( (*( p)) ) {
		case 78: goto st622;
		case 110: goto st622;
	}
	goto tr237;
st622:
	if ( ++( p) == ( pe) )
		goto _test_eof622;
case 622:
	switch( (*( p)) ) {
		case 71: goto st623;
		case 103: goto st623;
	}
	goto tr237;
st623:
	if ( ++( p) == ( pe) )
		goto _test_eof623;
case 623:
	if ( (*( p)) == 32 )
		goto st624;
	goto tr237;
st624:
	if ( ++( p) == ( pe) )
		goto _test_eof624;
case 624:
	if ( (*( p)) == 35 )
		goto st625;
	goto tr237;
st625:
	if ( ++( p) == ( pe) )
		goto _test_eof625;
case 625:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr780;
	goto tr237;
tr780:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1713;
st1713:
	if ( ++( p) == ( pe) )
		goto _test_eof1713;
case 1713:
#line 13843 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1713;
	goto tr2177;
tr2083:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1714;
st1714:
	if ( ++( p) == ( pe) )
		goto _test_eof1714;
case 1714:
#line 13859 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2179;
		case 85: goto tr2180;
		case 91: goto tr2132;
		case 97: goto tr2179;
		case 117: goto tr2180;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2179:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1715;
st1715:
	if ( ++( p) == ( pe) )
		goto _test_eof1715;
case 1715:
#line 13887 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2181;
		case 91: goto tr2132;
		case 110: goto tr2181;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2181:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1716;
st1716:
	if ( ++( p) == ( pe) )
		goto _test_eof1716;
case 1716:
#line 13913 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st626;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st626:
	if ( ++( p) == ( pe) )
		goto _test_eof626;
case 626:
	if ( (*( p)) == 35 )
		goto st627;
	goto tr237;
st627:
	if ( ++( p) == ( pe) )
		goto _test_eof627;
case 627:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr782;
	goto tr237;
tr782:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1717;
st1717:
	if ( ++( p) == ( pe) )
		goto _test_eof1717;
case 1717:
#line 13950 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1717;
	goto tr2183;
tr2180:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1718;
st1718:
	if ( ++( p) == ( pe) )
		goto _test_eof1718;
case 1718:
#line 13964 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2185;
		case 91: goto tr2132;
		case 114: goto tr2185;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2185:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1719;
st1719:
	if ( ++( p) == ( pe) )
		goto _test_eof1719;
case 1719:
#line 13990 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st628;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st628:
	if ( ++( p) == ( pe) )
		goto _test_eof628;
case 628:
	if ( (*( p)) == 35 )
		goto st629;
	goto tr237;
st629:
	if ( ++( p) == ( pe) )
		goto _test_eof629;
case 629:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr784;
	goto tr237;
tr784:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1720;
st1720:
	if ( ++( p) == ( pe) )
		goto _test_eof1720;
case 1720:
#line 14027 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1720;
	goto tr2187;
tr2084:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1721;
st1721:
	if ( ++( p) == ( pe) )
		goto _test_eof1721;
case 1721:
#line 14043 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2189;
		case 91: goto tr2132;
		case 111: goto tr2189;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2189:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1722;
st1722:
	if ( ++( p) == ( pe) )
		goto _test_eof1722;
case 1722:
#line 14069 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2190;
		case 91: goto tr2132;
		case 109: goto tr2190;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2190:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1723;
st1723:
	if ( ++( p) == ( pe) )
		goto _test_eof1723;
case 1723:
#line 14095 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2191;
		case 91: goto tr2132;
		case 109: goto tr2191;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2191:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1724;
st1724:
	if ( ++( p) == ( pe) )
		goto _test_eof1724;
case 1724:
#line 14121 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2192;
		case 73: goto tr2193;
		case 91: goto tr2132;
		case 101: goto tr2192;
		case 105: goto tr2193;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2192:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1725;
st1725:
	if ( ++( p) == ( pe) )
		goto _test_eof1725;
case 1725:
#line 14149 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2194;
		case 91: goto tr2132;
		case 110: goto tr2194;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2194:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1726;
st1726:
	if ( ++( p) == ( pe) )
		goto _test_eof1726;
case 1726:
#line 14175 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2195;
		case 91: goto tr2132;
		case 116: goto tr2195;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2195:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1727;
st1727:
	if ( ++( p) == ( pe) )
		goto _test_eof1727;
case 1727:
#line 14201 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st630;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st630:
	if ( ++( p) == ( pe) )
		goto _test_eof630;
case 630:
	if ( (*( p)) == 35 )
		goto st631;
	goto tr237;
st631:
	if ( ++( p) == ( pe) )
		goto _test_eof631;
case 631:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr786;
	goto tr237;
tr786:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1728;
st1728:
	if ( ++( p) == ( pe) )
		goto _test_eof1728;
case 1728:
#line 14238 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1728;
	goto tr2197;
tr2193:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1729;
st1729:
	if ( ++( p) == ( pe) )
		goto _test_eof1729;
case 1729:
#line 14252 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2199;
		case 91: goto tr2132;
		case 116: goto tr2199;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2199:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1730;
st1730:
	if ( ++( p) == ( pe) )
		goto _test_eof1730;
case 1730:
#line 14278 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st632;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st632:
	if ( ++( p) == ( pe) )
		goto _test_eof632;
case 632:
	if ( (*( p)) == 35 )
		goto st633;
	goto tr237;
st633:
	if ( ++( p) == ( pe) )
		goto _test_eof633;
case 633:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr788;
	goto tr237;
tr788:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1731;
st1731:
	if ( ++( p) == ( pe) )
		goto _test_eof1731;
case 1731:
#line 14315 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1731;
	goto tr2201;
tr2085:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1732;
st1732:
	if ( ++( p) == ( pe) )
		goto _test_eof1732;
case 1732:
#line 14331 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2203;
		case 78: goto tr2204;
		case 91: goto tr2132;
		case 109: goto tr2203;
		case 110: goto tr2204;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2203:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1733;
st1733:
	if ( ++( p) == ( pe) )
		goto _test_eof1733;
case 1733:
#line 14359 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2205;
		case 91: goto tr2132;
		case 97: goto tr2205;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2205:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1734;
st1734:
	if ( ++( p) == ( pe) )
		goto _test_eof1734;
case 1734:
#line 14385 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2206;
		case 91: goto tr2132;
		case 105: goto tr2206;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2206:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1735;
st1735:
	if ( ++( p) == ( pe) )
		goto _test_eof1735;
case 1735:
#line 14411 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2207;
		case 91: goto tr2132;
		case 108: goto tr2207;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2207:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1736;
st1736:
	if ( ++( p) == ( pe) )
		goto _test_eof1736;
case 1736:
#line 14437 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st634;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st634:
	if ( ++( p) == ( pe) )
		goto _test_eof634;
case 634:
	if ( (*( p)) == 35 )
		goto st635;
	goto tr237;
st635:
	if ( ++( p) == ( pe) )
		goto _test_eof635;
case 635:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr790;
	goto tr237;
tr2211:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1737;
tr790:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1737;
st1737:
	if ( ++( p) == ( pe) )
		goto _test_eof1737;
case 1737:
#line 14480 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto tr2210;
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr2211;
	goto tr2209;
tr2210:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st636;
st636:
	if ( ++( p) == ( pe) )
		goto _test_eof636;
case 636:
#line 14494 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 45: goto tr792;
		case 61: goto tr792;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr792;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr792;
	} else
		goto tr792;
	goto tr791;
tr792:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1738;
st1738:
	if ( ++( p) == ( pe) )
		goto _test_eof1738;
case 1738:
#line 14516 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 45: goto st1738;
		case 61: goto st1738;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1738;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1738;
	} else
		goto st1738;
	goto tr2212;
tr2204:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1739;
st1739:
	if ( ++( p) == ( pe) )
		goto _test_eof1739;
case 1739:
#line 14540 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2214;
		case 91: goto tr2132;
		case 112: goto tr2214;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2214:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1740;
st1740:
	if ( ++( p) == ( pe) )
		goto _test_eof1740;
case 1740:
#line 14566 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st637;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st637:
	if ( ++( p) == ( pe) )
		goto _test_eof637;
case 637:
	if ( (*( p)) == 35 )
		goto st638;
	goto tr237;
st638:
	if ( ++( p) == ( pe) )
		goto _test_eof638;
case 638:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr794;
	goto tr237;
tr794:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1741;
st1741:
	if ( ++( p) == ( pe) )
		goto _test_eof1741;
case 1741:
#line 14603 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1741;
	goto tr2216;
tr2086:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1742;
st1742:
	if ( ++( p) == ( pe) )
		goto _test_eof1742;
case 1742:
#line 14619 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2218;
		case 79: goto tr2219;
		case 91: goto tr2132;
		case 108: goto tr2218;
		case 111: goto tr2219;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2218:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1743;
st1743:
	if ( ++( p) == ( pe) )
		goto _test_eof1743;
case 1743:
#line 14647 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2220;
		case 91: goto tr2132;
		case 97: goto tr2220;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2220:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1744;
st1744:
	if ( ++( p) == ( pe) )
		goto _test_eof1744;
case 1744:
#line 14673 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2221;
		case 91: goto tr2132;
		case 103: goto tr2221;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2221:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1745;
st1745:
	if ( ++( p) == ( pe) )
		goto _test_eof1745;
case 1745:
#line 14699 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st639;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st639:
	if ( ++( p) == ( pe) )
		goto _test_eof639;
case 639:
	if ( (*( p)) == 35 )
		goto st640;
	goto tr237;
st640:
	if ( ++( p) == ( pe) )
		goto _test_eof640;
case 640:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr796;
	goto tr237;
tr796:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1746;
st1746:
	if ( ++( p) == ( pe) )
		goto _test_eof1746;
case 1746:
#line 14736 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1746;
	goto tr2223;
tr2219:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1747;
st1747:
	if ( ++( p) == ( pe) )
		goto _test_eof1747;
case 1747:
#line 14750 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2225;
		case 91: goto tr2132;
		case 114: goto tr2225;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2225:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1748;
st1748:
	if ( ++( p) == ( pe) )
		goto _test_eof1748;
case 1748:
#line 14776 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2226;
		case 91: goto tr2132;
		case 117: goto tr2226;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2226:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1749;
st1749:
	if ( ++( p) == ( pe) )
		goto _test_eof1749;
case 1749:
#line 14802 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2227;
		case 91: goto tr2132;
		case 109: goto tr2227;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2227:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1750;
st1750:
	if ( ++( p) == ( pe) )
		goto _test_eof1750;
case 1750:
#line 14828 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st641;
		case 80: goto tr2229;
		case 91: goto tr2132;
		case 112: goto tr2229;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st641:
	if ( ++( p) == ( pe) )
		goto _test_eof641;
case 641:
	switch( (*( p)) ) {
		case 32: goto st642;
		case 35: goto st643;
		case 80: goto st644;
		case 84: goto st648;
		case 112: goto st644;
		case 116: goto st648;
	}
	goto tr237;
st642:
	if ( ++( p) == ( pe) )
		goto _test_eof642;
case 642:
	if ( (*( p)) == 35 )
		goto st643;
	goto tr237;
st643:
	if ( ++( p) == ( pe) )
		goto _test_eof643;
case 643:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr801;
	goto tr237;
tr801:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1751;
st1751:
	if ( ++( p) == ( pe) )
		goto _test_eof1751;
case 1751:
#line 14880 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1751;
	goto tr2230;
st644:
	if ( ++( p) == ( pe) )
		goto _test_eof644;
case 644:
	switch( (*( p)) ) {
		case 79: goto st645;
		case 111: goto st645;
	}
	goto tr237;
st645:
	if ( ++( p) == ( pe) )
		goto _test_eof645;
case 645:
	switch( (*( p)) ) {
		case 83: goto st646;
		case 115: goto st646;
	}
	goto tr237;
st646:
	if ( ++( p) == ( pe) )
		goto _test_eof646;
case 646:
	switch( (*( p)) ) {
		case 84: goto st647;
		case 116: goto st647;
	}
	goto tr237;
st647:
	if ( ++( p) == ( pe) )
		goto _test_eof647;
case 647:
	if ( (*( p)) == 32 )
		goto st642;
	goto tr237;
st648:
	if ( ++( p) == ( pe) )
		goto _test_eof648;
case 648:
	switch( (*( p)) ) {
		case 79: goto st649;
		case 111: goto st649;
	}
	goto tr237;
st649:
	if ( ++( p) == ( pe) )
		goto _test_eof649;
case 649:
	switch( (*( p)) ) {
		case 80: goto st650;
		case 112: goto st650;
	}
	goto tr237;
st650:
	if ( ++( p) == ( pe) )
		goto _test_eof650;
case 650:
	switch( (*( p)) ) {
		case 73: goto st651;
		case 105: goto st651;
	}
	goto tr237;
st651:
	if ( ++( p) == ( pe) )
		goto _test_eof651;
case 651:
	switch( (*( p)) ) {
		case 67: goto st652;
		case 99: goto st652;
	}
	goto tr237;
st652:
	if ( ++( p) == ( pe) )
		goto _test_eof652;
case 652:
	if ( (*( p)) == 32 )
		goto st653;
	goto tr237;
st653:
	if ( ++( p) == ( pe) )
		goto _test_eof653;
case 653:
	if ( (*( p)) == 35 )
		goto st654;
	goto tr237;
st654:
	if ( ++( p) == ( pe) )
		goto _test_eof654;
case 654:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr811;
	goto tr237;
tr2234:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1752;
tr811:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1752;
st1752:
	if ( ++( p) == ( pe) )
		goto _test_eof1752;
case 1752:
#line 14989 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto tr2233;
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr2234;
	goto tr2232;
tr2233:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st655;
st655:
	if ( ++( p) == ( pe) )
		goto _test_eof655;
case 655:
#line 15003 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto st656;
		case 112: goto st656;
	}
	goto tr812;
st656:
	if ( ++( p) == ( pe) )
		goto _test_eof656;
case 656:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr814;
	goto tr812;
tr814:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1753;
st1753:
	if ( ++( p) == ( pe) )
		goto _test_eof1753;
case 1753:
#line 15024 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1753;
	goto tr2235;
tr2229:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1754;
st1754:
	if ( ++( p) == ( pe) )
		goto _test_eof1754;
case 1754:
#line 15038 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2237;
		case 91: goto tr2132;
		case 111: goto tr2237;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2237:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1755;
st1755:
	if ( ++( p) == ( pe) )
		goto _test_eof1755;
case 1755:
#line 15064 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2238;
		case 91: goto tr2132;
		case 115: goto tr2238;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2238:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1756;
st1756:
	if ( ++( p) == ( pe) )
		goto _test_eof1756;
case 1756:
#line 15090 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2239;
		case 91: goto tr2132;
		case 116: goto tr2239;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2239:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1757;
st1757:
	if ( ++( p) == ( pe) )
		goto _test_eof1757;
case 1757:
#line 15116 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st642;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2087:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1758;
st1758:
	if ( ++( p) == ( pe) )
		goto _test_eof1758;
case 1758:
#line 15143 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2240;
		case 91: goto tr2132;
		case 116: goto tr2240;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2240:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1759;
st1759:
	if ( ++( p) == ( pe) )
		goto _test_eof1759;
case 1759:
#line 15169 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2241;
		case 91: goto tr2132;
		case 116: goto tr2241;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2241:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1760;
st1760:
	if ( ++( p) == ( pe) )
		goto _test_eof1760;
case 1760:
#line 15195 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2242;
		case 91: goto tr2132;
		case 112: goto tr2242;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2242:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1761;
st1761:
	if ( ++( p) == ( pe) )
		goto _test_eof1761;
case 1761:
#line 15221 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 58: goto st657;
		case 83: goto tr2244;
		case 91: goto tr2132;
		case 115: goto tr2244;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st657:
	if ( ++( p) == ( pe) )
		goto _test_eof657;
case 657:
	if ( (*( p)) == 47 )
		goto st658;
	goto tr237;
st658:
	if ( ++( p) == ( pe) )
		goto _test_eof658;
case 658:
	if ( (*( p)) == 47 )
		goto st659;
	goto tr237;
st659:
	if ( ++( p) == ( pe) )
		goto _test_eof659;
case 659:
	switch( (*( p)) ) {
		case 45: goto st661;
		case 95: goto st661;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st662;
		} else if ( (*( p)) >= -62 )
			goto st660;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st661;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st661;
		} else
			goto st661;
	} else
		goto st663;
	goto tr237;
st660:
	if ( ++( p) == ( pe) )
		goto _test_eof660;
case 660:
	if ( (*( p)) <= -65 )
		goto st661;
	goto tr237;
st661:
	if ( ++( p) == ( pe) )
		goto _test_eof661;
case 661:
	switch( (*( p)) ) {
		case 45: goto st661;
		case 46: goto st664;
		case 95: goto st661;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st662;
		} else if ( (*( p)) >= -62 )
			goto st660;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st661;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st661;
		} else
			goto st661;
	} else
		goto st663;
	goto tr237;
st662:
	if ( ++( p) == ( pe) )
		goto _test_eof662;
case 662:
	if ( (*( p)) <= -65 )
		goto st660;
	goto tr237;
st663:
	if ( ++( p) == ( pe) )
		goto _test_eof663;
case 663:
	if ( (*( p)) <= -65 )
		goto st662;
	goto tr237;
st664:
	if ( ++( p) == ( pe) )
		goto _test_eof664;
case 664:
	switch( (*( p)) ) {
		case -30: goto st667;
		case -29: goto st670;
		case -17: goto st672;
		case 45: goto tr828;
		case 95: goto tr828;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st666;
		} else if ( (*( p)) >= -62 )
			goto st665;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto tr828;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr828;
		} else
			goto tr828;
	} else
		goto st675;
	goto tr234;
st665:
	if ( ++( p) == ( pe) )
		goto _test_eof665;
case 665:
	if ( (*( p)) <= -65 )
		goto tr828;
	goto tr234;
tr828:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 389 "ext/dtext/dtext.cpp.rl"
	{( act) = 50;}
	goto st1762;
st1762:
	if ( ++( p) == ( pe) )
		goto _test_eof1762;
case 1762:
#line 15372 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st667;
		case -29: goto st670;
		case -17: goto st672;
		case 35: goto tr831;
		case 46: goto st664;
		case 47: goto tr832;
		case 58: goto st709;
		case 63: goto st698;
		case 95: goto tr828;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st666;
		} else if ( (*( p)) >= -62 )
			goto st665;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr828;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr828;
		} else
			goto tr828;
	} else
		goto st675;
	goto tr2245;
st666:
	if ( ++( p) == ( pe) )
		goto _test_eof666;
case 666:
	if ( (*( p)) <= -65 )
		goto st665;
	goto tr234;
st667:
	if ( ++( p) == ( pe) )
		goto _test_eof667;
case 667:
	if ( (*( p)) == -99 )
		goto st668;
	if ( (*( p)) <= -65 )
		goto st665;
	goto tr234;
st668:
	if ( ++( p) == ( pe) )
		goto _test_eof668;
case 668:
	if ( (*( p)) == -83 )
		goto st669;
	if ( (*( p)) <= -65 )
		goto tr828;
	goto tr234;
st669:
	if ( ++( p) == ( pe) )
		goto _test_eof669;
case 669:
	switch( (*( p)) ) {
		case -30: goto st667;
		case -29: goto st670;
		case -17: goto st672;
		case 35: goto tr831;
		case 46: goto st664;
		case 47: goto tr832;
		case 58: goto st709;
		case 63: goto st698;
		case 95: goto tr828;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st666;
		} else if ( (*( p)) >= -62 )
			goto st665;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr828;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr828;
		} else
			goto tr828;
	} else
		goto st675;
	goto tr234;
st670:
	if ( ++( p) == ( pe) )
		goto _test_eof670;
case 670:
	if ( (*( p)) == -128 )
		goto st671;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st665;
	goto tr234;
st671:
	if ( ++( p) == ( pe) )
		goto _test_eof671;
case 671:
	if ( (*( p)) < -120 ) {
		if ( (*( p)) > -126 ) {
			if ( -125 <= (*( p)) && (*( p)) <= -121 )
				goto tr828;
		} else
			goto st669;
	} else if ( (*( p)) > -111 ) {
		if ( (*( p)) < -108 ) {
			if ( -110 <= (*( p)) && (*( p)) <= -109 )
				goto tr828;
		} else if ( (*( p)) > -100 ) {
			if ( -99 <= (*( p)) && (*( p)) <= -65 )
				goto tr828;
		} else
			goto st669;
	} else
		goto st669;
	goto tr234;
st672:
	if ( ++( p) == ( pe) )
		goto _test_eof672;
case 672:
	switch( (*( p)) ) {
		case -68: goto st673;
		case -67: goto st674;
	}
	if ( (*( p)) <= -65 )
		goto st665;
	goto tr234;
st673:
	if ( ++( p) == ( pe) )
		goto _test_eof673;
case 673:
	switch( (*( p)) ) {
		case -119: goto st669;
		case -67: goto st669;
	}
	if ( (*( p)) <= -65 )
		goto tr828;
	goto tr234;
st674:
	if ( ++( p) == ( pe) )
		goto _test_eof674;
case 674:
	switch( (*( p)) ) {
		case -99: goto st669;
		case -96: goto st669;
		case -93: goto st669;
	}
	if ( (*( p)) <= -65 )
		goto tr828;
	goto tr234;
st675:
	if ( ++( p) == ( pe) )
		goto _test_eof675;
case 675:
	if ( (*( p)) <= -65 )
		goto st666;
	goto tr234;
tr831:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1763;
st1763:
	if ( ++( p) == ( pe) )
		goto _test_eof1763;
case 1763:
#line 15540 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st678;
		case -29: goto st680;
		case -17: goto st682;
		case 32: goto tr2245;
		case 34: goto st686;
		case 35: goto tr2245;
		case 39: goto st686;
		case 44: goto st686;
		case 46: goto st686;
		case 60: goto tr2245;
		case 62: goto tr2245;
		case 63: goto st686;
		case 91: goto tr2245;
		case 93: goto tr2245;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2245;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st677;
		} else
			goto st676;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2245;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st686;
		} else
			goto tr2245;
	} else
		goto st685;
	goto tr831;
st676:
	if ( ++( p) == ( pe) )
		goto _test_eof676;
case 676:
	if ( (*( p)) <= -65 )
		goto tr831;
	goto tr838;
st677:
	if ( ++( p) == ( pe) )
		goto _test_eof677;
case 677:
	if ( (*( p)) <= -65 )
		goto st676;
	goto tr838;
st678:
	if ( ++( p) == ( pe) )
		goto _test_eof678;
case 678:
	if ( (*( p)) == -99 )
		goto st679;
	if ( (*( p)) <= -65 )
		goto st676;
	goto tr838;
st679:
	if ( ++( p) == ( pe) )
		goto _test_eof679;
case 679:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr831;
	} else
		goto tr831;
	goto tr838;
st680:
	if ( ++( p) == ( pe) )
		goto _test_eof680;
case 680:
	if ( (*( p)) == -128 )
		goto st681;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st676;
	goto tr838;
st681:
	if ( ++( p) == ( pe) )
		goto _test_eof681;
case 681:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr831;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr831;
	} else
		goto tr831;
	goto tr838;
st682:
	if ( ++( p) == ( pe) )
		goto _test_eof682;
case 682:
	switch( (*( p)) ) {
		case -68: goto st683;
		case -67: goto st684;
	}
	if ( (*( p)) <= -65 )
		goto st676;
	goto tr838;
st683:
	if ( ++( p) == ( pe) )
		goto _test_eof683;
case 683:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr831;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr831;
	} else
		goto tr831;
	goto tr838;
st684:
	if ( ++( p) == ( pe) )
		goto _test_eof684;
case 684:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr831;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr831;
		} else if ( (*( p)) >= -95 )
			goto tr831;
	} else
		goto tr831;
	goto tr838;
st685:
	if ( ++( p) == ( pe) )
		goto _test_eof685;
case 685:
	if ( (*( p)) <= -65 )
		goto st677;
	goto tr838;
st686:
	if ( ++( p) == ( pe) )
		goto _test_eof686;
case 686:
	switch( (*( p)) ) {
		case -30: goto st678;
		case -29: goto st680;
		case -17: goto st682;
		case 32: goto tr838;
		case 34: goto st686;
		case 35: goto tr838;
		case 39: goto st686;
		case 44: goto st686;
		case 46: goto st686;
		case 60: goto tr838;
		case 62: goto tr838;
		case 63: goto st686;
		case 91: goto tr838;
		case 93: goto tr838;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr838;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st677;
		} else
			goto st676;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr838;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st686;
		} else
			goto tr838;
	} else
		goto st685;
	goto tr831;
tr832:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 389 "ext/dtext/dtext.cpp.rl"
	{( act) = 50;}
	goto st1764;
st1764:
	if ( ++( p) == ( pe) )
		goto _test_eof1764;
case 1764:
#line 15731 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st689;
		case -29: goto st691;
		case -17: goto st693;
		case 32: goto tr2245;
		case 34: goto st697;
		case 35: goto tr831;
		case 39: goto st697;
		case 44: goto st697;
		case 46: goto st697;
		case 60: goto tr2245;
		case 62: goto tr2245;
		case 63: goto st698;
		case 91: goto tr2245;
		case 93: goto tr2245;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2245;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st688;
		} else
			goto st687;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2245;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st697;
		} else
			goto tr2245;
	} else
		goto st696;
	goto tr832;
st687:
	if ( ++( p) == ( pe) )
		goto _test_eof687;
case 687:
	if ( (*( p)) <= -65 )
		goto tr832;
	goto tr838;
st688:
	if ( ++( p) == ( pe) )
		goto _test_eof688;
case 688:
	if ( (*( p)) <= -65 )
		goto st687;
	goto tr838;
st689:
	if ( ++( p) == ( pe) )
		goto _test_eof689;
case 689:
	if ( (*( p)) == -99 )
		goto st690;
	if ( (*( p)) <= -65 )
		goto st687;
	goto tr838;
st690:
	if ( ++( p) == ( pe) )
		goto _test_eof690;
case 690:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr832;
	} else
		goto tr832;
	goto tr838;
st691:
	if ( ++( p) == ( pe) )
		goto _test_eof691;
case 691:
	if ( (*( p)) == -128 )
		goto st692;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st687;
	goto tr838;
st692:
	if ( ++( p) == ( pe) )
		goto _test_eof692;
case 692:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr832;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr832;
	} else
		goto tr832;
	goto tr838;
st693:
	if ( ++( p) == ( pe) )
		goto _test_eof693;
case 693:
	switch( (*( p)) ) {
		case -68: goto st694;
		case -67: goto st695;
	}
	if ( (*( p)) <= -65 )
		goto st687;
	goto tr838;
st694:
	if ( ++( p) == ( pe) )
		goto _test_eof694;
case 694:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr832;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr832;
	} else
		goto tr832;
	goto tr838;
st695:
	if ( ++( p) == ( pe) )
		goto _test_eof695;
case 695:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr832;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr832;
		} else if ( (*( p)) >= -95 )
			goto tr832;
	} else
		goto tr832;
	goto tr838;
st696:
	if ( ++( p) == ( pe) )
		goto _test_eof696;
case 696:
	if ( (*( p)) <= -65 )
		goto st688;
	goto tr838;
st697:
	if ( ++( p) == ( pe) )
		goto _test_eof697;
case 697:
	switch( (*( p)) ) {
		case -30: goto st689;
		case -29: goto st691;
		case -17: goto st693;
		case 32: goto tr838;
		case 34: goto st697;
		case 35: goto tr831;
		case 39: goto st697;
		case 44: goto st697;
		case 46: goto st697;
		case 60: goto tr838;
		case 62: goto tr838;
		case 63: goto st698;
		case 91: goto tr838;
		case 93: goto tr838;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr838;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st688;
		} else
			goto st687;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr838;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st697;
		} else
			goto tr838;
	} else
		goto st696;
	goto tr832;
st698:
	if ( ++( p) == ( pe) )
		goto _test_eof698;
case 698:
	switch( (*( p)) ) {
		case -30: goto st701;
		case -29: goto st703;
		case -17: goto st705;
		case 32: goto tr234;
		case 34: goto st698;
		case 35: goto tr831;
		case 39: goto st698;
		case 44: goto st698;
		case 46: goto st698;
		case 63: goto st698;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr234;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st700;
		} else
			goto st699;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr234;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st698;
		} else
			goto tr234;
	} else
		goto st708;
	goto tr867;
st699:
	if ( ++( p) == ( pe) )
		goto _test_eof699;
case 699:
	if ( (*( p)) <= -65 )
		goto tr867;
	goto tr234;
tr867:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 389 "ext/dtext/dtext.cpp.rl"
	{( act) = 50;}
	goto st1765;
st1765:
	if ( ++( p) == ( pe) )
		goto _test_eof1765;
case 1765:
#line 15966 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st701;
		case -29: goto st703;
		case -17: goto st705;
		case 32: goto tr2245;
		case 34: goto st698;
		case 35: goto tr831;
		case 39: goto st698;
		case 44: goto st698;
		case 46: goto st698;
		case 63: goto st698;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2245;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st700;
		} else
			goto st699;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2245;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st698;
		} else
			goto tr2245;
	} else
		goto st708;
	goto tr867;
st700:
	if ( ++( p) == ( pe) )
		goto _test_eof700;
case 700:
	if ( (*( p)) <= -65 )
		goto st699;
	goto tr234;
st701:
	if ( ++( p) == ( pe) )
		goto _test_eof701;
case 701:
	if ( (*( p)) == -99 )
		goto st702;
	if ( (*( p)) <= -65 )
		goto st699;
	goto tr234;
st702:
	if ( ++( p) == ( pe) )
		goto _test_eof702;
case 702:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr867;
	} else
		goto tr867;
	goto tr234;
st703:
	if ( ++( p) == ( pe) )
		goto _test_eof703;
case 703:
	if ( (*( p)) == -128 )
		goto st704;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st699;
	goto tr234;
st704:
	if ( ++( p) == ( pe) )
		goto _test_eof704;
case 704:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr867;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr867;
	} else
		goto tr867;
	goto tr234;
st705:
	if ( ++( p) == ( pe) )
		goto _test_eof705;
case 705:
	switch( (*( p)) ) {
		case -68: goto st706;
		case -67: goto st707;
	}
	if ( (*( p)) <= -65 )
		goto st699;
	goto tr234;
st706:
	if ( ++( p) == ( pe) )
		goto _test_eof706;
case 706:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr867;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr867;
	} else
		goto tr867;
	goto tr234;
st707:
	if ( ++( p) == ( pe) )
		goto _test_eof707;
case 707:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr867;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr867;
		} else if ( (*( p)) >= -95 )
			goto tr867;
	} else
		goto tr867;
	goto tr234;
st708:
	if ( ++( p) == ( pe) )
		goto _test_eof708;
case 708:
	if ( (*( p)) <= -65 )
		goto st700;
	goto tr234;
st709:
	if ( ++( p) == ( pe) )
		goto _test_eof709;
case 709:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr872;
	goto tr234;
tr872:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 389 "ext/dtext/dtext.cpp.rl"
	{( act) = 50;}
	goto st1766;
st1766:
	if ( ++( p) == ( pe) )
		goto _test_eof1766;
case 1766:
#line 16112 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr831;
		case 47: goto tr832;
		case 63: goto st698;
	}
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr872;
	goto tr2245;
tr2244:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1767;
st1767:
	if ( ++( p) == ( pe) )
		goto _test_eof1767;
case 1767:
#line 16131 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 58: goto st657;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2088:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1768;
st1768:
	if ( ++( p) == ( pe) )
		goto _test_eof1768;
case 1768:
#line 16158 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2246;
		case 83: goto tr2247;
		case 91: goto tr2132;
		case 109: goto tr2246;
		case 115: goto tr2247;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2246:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1769;
st1769:
	if ( ++( p) == ( pe) )
		goto _test_eof1769;
case 1769:
#line 16186 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2248;
		case 91: goto tr2132;
		case 112: goto tr2248;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2248:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1770;
st1770:
	if ( ++( p) == ( pe) )
		goto _test_eof1770;
case 1770:
#line 16212 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2249;
		case 91: goto tr2132;
		case 108: goto tr2249;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2249:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1771;
st1771:
	if ( ++( p) == ( pe) )
		goto _test_eof1771;
case 1771:
#line 16238 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2250;
		case 91: goto tr2132;
		case 105: goto tr2250;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2250:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1772;
st1772:
	if ( ++( p) == ( pe) )
		goto _test_eof1772;
case 1772:
#line 16264 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2251;
		case 91: goto tr2132;
		case 99: goto tr2251;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2251:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1773;
st1773:
	if ( ++( p) == ( pe) )
		goto _test_eof1773;
case 1773:
#line 16290 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2252;
		case 91: goto tr2132;
		case 97: goto tr2252;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2252:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1774;
st1774:
	if ( ++( p) == ( pe) )
		goto _test_eof1774;
case 1774:
#line 16316 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2253;
		case 91: goto tr2132;
		case 116: goto tr2253;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2253:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1775;
st1775:
	if ( ++( p) == ( pe) )
		goto _test_eof1775;
case 1775:
#line 16342 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2254;
		case 91: goto tr2132;
		case 105: goto tr2254;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2254:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1776;
st1776:
	if ( ++( p) == ( pe) )
		goto _test_eof1776;
case 1776:
#line 16368 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2255;
		case 91: goto tr2132;
		case 111: goto tr2255;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2255:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1777;
st1777:
	if ( ++( p) == ( pe) )
		goto _test_eof1777;
case 1777:
#line 16394 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2256;
		case 91: goto tr2132;
		case 110: goto tr2256;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2256:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1778;
st1778:
	if ( ++( p) == ( pe) )
		goto _test_eof1778;
case 1778:
#line 16420 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st710;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st710:
	if ( ++( p) == ( pe) )
		goto _test_eof710;
case 710:
	if ( (*( p)) == 35 )
		goto st711;
	goto tr237;
st711:
	if ( ++( p) == ( pe) )
		goto _test_eof711;
case 711:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr874;
	goto tr237;
tr874:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1779;
st1779:
	if ( ++( p) == ( pe) )
		goto _test_eof1779;
case 1779:
#line 16457 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1779;
	goto tr2258;
tr2247:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1780;
st1780:
	if ( ++( p) == ( pe) )
		goto _test_eof1780;
case 1780:
#line 16471 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2260;
		case 91: goto tr2132;
		case 115: goto tr2260;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2260:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1781;
st1781:
	if ( ++( p) == ( pe) )
		goto _test_eof1781;
case 1781:
#line 16497 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2261;
		case 91: goto tr2132;
		case 117: goto tr2261;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2261:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1782;
st1782:
	if ( ++( p) == ( pe) )
		goto _test_eof1782;
case 1782:
#line 16523 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2262;
		case 91: goto tr2132;
		case 101: goto tr2262;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2262:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1783;
st1783:
	if ( ++( p) == ( pe) )
		goto _test_eof1783;
case 1783:
#line 16549 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st712;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st712:
	if ( ++( p) == ( pe) )
		goto _test_eof712;
case 712:
	if ( (*( p)) == 35 )
		goto st713;
	goto tr237;
st713:
	if ( ++( p) == ( pe) )
		goto _test_eof713;
case 713:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr876;
	goto tr237;
tr876:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1784;
st1784:
	if ( ++( p) == ( pe) )
		goto _test_eof1784;
case 1784:
#line 16586 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1784;
	goto tr2264;
tr2089:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1785;
st1785:
	if ( ++( p) == ( pe) )
		goto _test_eof1785;
case 1785:
#line 16602 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2266;
		case 91: goto tr2132;
		case 111: goto tr2266;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2266:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1786;
st1786:
	if ( ++( p) == ( pe) )
		goto _test_eof1786;
case 1786:
#line 16628 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2267;
		case 91: goto tr2132;
		case 100: goto tr2267;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2267:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1787;
st1787:
	if ( ++( p) == ( pe) )
		goto _test_eof1787;
case 1787:
#line 16654 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st714;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st714:
	if ( ++( p) == ( pe) )
		goto _test_eof714;
case 714:
	switch( (*( p)) ) {
		case 65: goto st715;
		case 97: goto st715;
	}
	goto tr237;
st715:
	if ( ++( p) == ( pe) )
		goto _test_eof715;
case 715:
	switch( (*( p)) ) {
		case 67: goto st716;
		case 99: goto st716;
	}
	goto tr237;
st716:
	if ( ++( p) == ( pe) )
		goto _test_eof716;
case 716:
	switch( (*( p)) ) {
		case 84: goto st717;
		case 116: goto st717;
	}
	goto tr237;
st717:
	if ( ++( p) == ( pe) )
		goto _test_eof717;
case 717:
	switch( (*( p)) ) {
		case 73: goto st718;
		case 105: goto st718;
	}
	goto tr237;
st718:
	if ( ++( p) == ( pe) )
		goto _test_eof718;
case 718:
	switch( (*( p)) ) {
		case 79: goto st719;
		case 111: goto st719;
	}
	goto tr237;
st719:
	if ( ++( p) == ( pe) )
		goto _test_eof719;
case 719:
	switch( (*( p)) ) {
		case 78: goto st720;
		case 110: goto st720;
	}
	goto tr237;
st720:
	if ( ++( p) == ( pe) )
		goto _test_eof720;
case 720:
	if ( (*( p)) == 32 )
		goto st721;
	goto tr237;
st721:
	if ( ++( p) == ( pe) )
		goto _test_eof721;
case 721:
	if ( (*( p)) == 35 )
		goto st722;
	goto tr237;
st722:
	if ( ++( p) == ( pe) )
		goto _test_eof722;
case 722:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr885;
	goto tr237;
tr885:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1788;
st1788:
	if ( ++( p) == ( pe) )
		goto _test_eof1788;
case 1788:
#line 16752 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1788;
	goto tr2269;
tr2090:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1789;
st1789:
	if ( ++( p) == ( pe) )
		goto _test_eof1789;
case 1789:
#line 16768 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2271;
		case 91: goto tr2132;
		case 111: goto tr2271;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2271:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1790;
st1790:
	if ( ++( p) == ( pe) )
		goto _test_eof1790;
case 1790:
#line 16794 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2272;
		case 91: goto tr2132;
		case 116: goto tr2272;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2272:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1791;
st1791:
	if ( ++( p) == ( pe) )
		goto _test_eof1791;
case 1791:
#line 16820 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2273;
		case 91: goto tr2132;
		case 101: goto tr2273;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2273:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1792;
st1792:
	if ( ++( p) == ( pe) )
		goto _test_eof1792;
case 1792:
#line 16846 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st723;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st723:
	if ( ++( p) == ( pe) )
		goto _test_eof723;
case 723:
	if ( (*( p)) == 35 )
		goto st724;
	goto tr237;
st724:
	if ( ++( p) == ( pe) )
		goto _test_eof724;
case 724:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr887;
	goto tr237;
tr887:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1793;
st1793:
	if ( ++( p) == ( pe) )
		goto _test_eof1793;
case 1793:
#line 16883 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1793;
	goto tr2275;
tr2091:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1794;
st1794:
	if ( ++( p) == ( pe) )
		goto _test_eof1794;
case 1794:
#line 16899 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2277;
		case 85: goto tr2278;
		case 91: goto tr2132;
		case 111: goto tr2277;
		case 117: goto tr2278;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2277:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1795;
st1795:
	if ( ++( p) == ( pe) )
		goto _test_eof1795;
case 1795:
#line 16927 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2279;
		case 83: goto tr2280;
		case 91: goto tr2132;
		case 111: goto tr2279;
		case 115: goto tr2280;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2279:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1796;
st1796:
	if ( ++( p) == ( pe) )
		goto _test_eof1796;
case 1796:
#line 16955 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2281;
		case 91: goto tr2132;
		case 108: goto tr2281;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2281:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1797;
st1797:
	if ( ++( p) == ( pe) )
		goto _test_eof1797;
case 1797:
#line 16981 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st725;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st725:
	if ( ++( p) == ( pe) )
		goto _test_eof725;
case 725:
	if ( (*( p)) == 35 )
		goto st726;
	goto tr237;
st726:
	if ( ++( p) == ( pe) )
		goto _test_eof726;
case 726:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr889;
	goto tr237;
tr889:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1798;
st1798:
	if ( ++( p) == ( pe) )
		goto _test_eof1798;
case 1798:
#line 17018 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1798;
	goto tr2283;
tr2280:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1799;
st1799:
	if ( ++( p) == ( pe) )
		goto _test_eof1799;
case 1799:
#line 17032 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2285;
		case 91: goto tr2132;
		case 116: goto tr2285;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2285:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1800;
st1800:
	if ( ++( p) == ( pe) )
		goto _test_eof1800;
case 1800:
#line 17058 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st727;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st727:
	if ( ++( p) == ( pe) )
		goto _test_eof727;
case 727:
	switch( (*( p)) ) {
		case 35: goto st728;
		case 67: goto st729;
		case 99: goto st729;
	}
	goto tr237;
st728:
	if ( ++( p) == ( pe) )
		goto _test_eof728;
case 728:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr892;
	goto tr237;
tr892:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1801;
st1801:
	if ( ++( p) == ( pe) )
		goto _test_eof1801;
case 1801:
#line 17098 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1801;
	goto tr2287;
st729:
	if ( ++( p) == ( pe) )
		goto _test_eof729;
case 729:
	switch( (*( p)) ) {
		case 72: goto st730;
		case 104: goto st730;
	}
	goto tr237;
st730:
	if ( ++( p) == ( pe) )
		goto _test_eof730;
case 730:
	switch( (*( p)) ) {
		case 65: goto st731;
		case 97: goto st731;
	}
	goto tr237;
st731:
	if ( ++( p) == ( pe) )
		goto _test_eof731;
case 731:
	switch( (*( p)) ) {
		case 78: goto st732;
		case 110: goto st732;
	}
	goto tr237;
st732:
	if ( ++( p) == ( pe) )
		goto _test_eof732;
case 732:
	switch( (*( p)) ) {
		case 71: goto st733;
		case 103: goto st733;
	}
	goto tr237;
st733:
	if ( ++( p) == ( pe) )
		goto _test_eof733;
case 733:
	switch( (*( p)) ) {
		case 69: goto st734;
		case 101: goto st734;
	}
	goto tr237;
st734:
	if ( ++( p) == ( pe) )
		goto _test_eof734;
case 734:
	switch( (*( p)) ) {
		case 83: goto st735;
		case 115: goto st735;
	}
	goto tr237;
st735:
	if ( ++( p) == ( pe) )
		goto _test_eof735;
case 735:
	if ( (*( p)) == 32 )
		goto st736;
	goto tr237;
st736:
	if ( ++( p) == ( pe) )
		goto _test_eof736;
case 736:
	if ( (*( p)) == 35 )
		goto st737;
	goto tr237;
st737:
	if ( ++( p) == ( pe) )
		goto _test_eof737;
case 737:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr901;
	goto tr237;
tr901:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1802;
st1802:
	if ( ++( p) == ( pe) )
		goto _test_eof1802;
case 1802:
#line 17185 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1802;
	goto tr2289;
tr2278:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1803;
st1803:
	if ( ++( p) == ( pe) )
		goto _test_eof1803;
case 1803:
#line 17199 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2291;
		case 91: goto tr2132;
		case 108: goto tr2291;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2291:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1804;
st1804:
	if ( ++( p) == ( pe) )
		goto _test_eof1804;
case 1804:
#line 17225 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2292;
		case 91: goto tr2132;
		case 108: goto tr2292;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2292:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1805;
st1805:
	if ( ++( p) == ( pe) )
		goto _test_eof1805;
case 1805:
#line 17251 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st738;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st738:
	if ( ++( p) == ( pe) )
		goto _test_eof738;
case 738:
	if ( (*( p)) == 35 )
		goto st739;
	goto tr237;
st739:
	if ( ++( p) == ( pe) )
		goto _test_eof739;
case 739:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr903;
	goto tr237;
tr903:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1806;
st1806:
	if ( ++( p) == ( pe) )
		goto _test_eof1806;
case 1806:
#line 17288 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1806;
	goto tr2294;
tr2092:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1807;
st1807:
	if ( ++( p) == ( pe) )
		goto _test_eof1807;
case 1807:
#line 17304 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2296;
		case 91: goto tr2132;
		case 101: goto tr2296;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2296:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1808;
st1808:
	if ( ++( p) == ( pe) )
		goto _test_eof1808;
case 1808:
#line 17330 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2297;
		case 91: goto tr2132;
		case 99: goto tr2297;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2297:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1809;
st1809:
	if ( ++( p) == ( pe) )
		goto _test_eof1809;
case 1809:
#line 17356 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2298;
		case 91: goto tr2132;
		case 111: goto tr2298;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2298:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1810;
st1810:
	if ( ++( p) == ( pe) )
		goto _test_eof1810;
case 1810:
#line 17382 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2299;
		case 91: goto tr2132;
		case 114: goto tr2299;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2299:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1811;
st1811:
	if ( ++( p) == ( pe) )
		goto _test_eof1811;
case 1811:
#line 17408 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2300;
		case 91: goto tr2132;
		case 100: goto tr2300;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2300:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1812;
st1812:
	if ( ++( p) == ( pe) )
		goto _test_eof1812;
case 1812:
#line 17434 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st740;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st740:
	if ( ++( p) == ( pe) )
		goto _test_eof740;
case 740:
	if ( (*( p)) == 35 )
		goto st741;
	goto tr237;
st741:
	if ( ++( p) == ( pe) )
		goto _test_eof741;
case 741:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr905;
	goto tr237;
tr905:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1813;
st1813:
	if ( ++( p) == ( pe) )
		goto _test_eof1813;
case 1813:
#line 17471 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1813;
	goto tr2302;
tr2093:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1814;
st1814:
	if ( ++( p) == ( pe) )
		goto _test_eof1814;
case 1814:
#line 17487 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2304;
		case 91: goto tr2132;
		case 101: goto tr2304;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2304:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1815;
st1815:
	if ( ++( p) == ( pe) )
		goto _test_eof1815;
case 1815:
#line 17513 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2305;
		case 91: goto tr2132;
		case 116: goto tr2305;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2305:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1816;
st1816:
	if ( ++( p) == ( pe) )
		goto _test_eof1816;
case 1816:
#line 17539 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st742;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st742:
	if ( ++( p) == ( pe) )
		goto _test_eof742;
case 742:
	if ( (*( p)) == 35 )
		goto st743;
	goto tr237;
st743:
	if ( ++( p) == ( pe) )
		goto _test_eof743;
case 743:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr907;
	goto tr237;
tr907:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1817;
st1817:
	if ( ++( p) == ( pe) )
		goto _test_eof1817;
case 1817:
#line 17576 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1817;
	goto tr2307;
tr2094:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1818;
st1818:
	if ( ++( p) == ( pe) )
		goto _test_eof1818;
case 1818:
#line 17592 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2309;
		case 72: goto tr2310;
		case 73: goto tr2311;
		case 79: goto tr2312;
		case 91: goto tr2132;
		case 97: goto tr2309;
		case 104: goto tr2310;
		case 105: goto tr2311;
		case 111: goto tr2312;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2309:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1819;
st1819:
	if ( ++( p) == ( pe) )
		goto _test_eof1819;
case 1819:
#line 17624 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2313;
		case 91: goto tr2132;
		case 107: goto tr2313;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2313:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1820;
st1820:
	if ( ++( p) == ( pe) )
		goto _test_eof1820;
case 1820:
#line 17650 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2314;
		case 91: goto tr2132;
		case 101: goto tr2314;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2314:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1821;
st1821:
	if ( ++( p) == ( pe) )
		goto _test_eof1821;
case 1821:
#line 17676 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st744;
		case 68: goto tr2316;
		case 91: goto tr2132;
		case 100: goto tr2316;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st744:
	if ( ++( p) == ( pe) )
		goto _test_eof744;
case 744:
	switch( (*( p)) ) {
		case 68: goto st745;
		case 100: goto st745;
	}
	goto tr237;
st745:
	if ( ++( p) == ( pe) )
		goto _test_eof745;
case 745:
	switch( (*( p)) ) {
		case 79: goto st746;
		case 111: goto st746;
	}
	goto tr237;
st746:
	if ( ++( p) == ( pe) )
		goto _test_eof746;
case 746:
	switch( (*( p)) ) {
		case 87: goto st747;
		case 119: goto st747;
	}
	goto tr237;
st747:
	if ( ++( p) == ( pe) )
		goto _test_eof747;
case 747:
	switch( (*( p)) ) {
		case 78: goto st748;
		case 110: goto st748;
	}
	goto tr237;
st748:
	if ( ++( p) == ( pe) )
		goto _test_eof748;
case 748:
	if ( (*( p)) == 32 )
		goto st749;
	goto tr237;
st749:
	if ( ++( p) == ( pe) )
		goto _test_eof749;
case 749:
	switch( (*( p)) ) {
		case 35: goto st750;
		case 82: goto st751;
		case 114: goto st751;
	}
	goto tr237;
st750:
	if ( ++( p) == ( pe) )
		goto _test_eof750;
case 750:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr915;
	goto tr237;
tr915:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1822;
st1822:
	if ( ++( p) == ( pe) )
		goto _test_eof1822;
case 1822:
#line 17761 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1822;
	goto tr2317;
st751:
	if ( ++( p) == ( pe) )
		goto _test_eof751;
case 751:
	switch( (*( p)) ) {
		case 69: goto st752;
		case 101: goto st752;
	}
	goto tr237;
st752:
	if ( ++( p) == ( pe) )
		goto _test_eof752;
case 752:
	switch( (*( p)) ) {
		case 81: goto st753;
		case 113: goto st753;
	}
	goto tr237;
st753:
	if ( ++( p) == ( pe) )
		goto _test_eof753;
case 753:
	switch( (*( p)) ) {
		case 85: goto st754;
		case 117: goto st754;
	}
	goto tr237;
st754:
	if ( ++( p) == ( pe) )
		goto _test_eof754;
case 754:
	switch( (*( p)) ) {
		case 69: goto st755;
		case 101: goto st755;
	}
	goto tr237;
st755:
	if ( ++( p) == ( pe) )
		goto _test_eof755;
case 755:
	switch( (*( p)) ) {
		case 83: goto st756;
		case 115: goto st756;
	}
	goto tr237;
st756:
	if ( ++( p) == ( pe) )
		goto _test_eof756;
case 756:
	switch( (*( p)) ) {
		case 84: goto st757;
		case 116: goto st757;
	}
	goto tr237;
st757:
	if ( ++( p) == ( pe) )
		goto _test_eof757;
case 757:
	if ( (*( p)) == 32 )
		goto st758;
	goto tr237;
st758:
	if ( ++( p) == ( pe) )
		goto _test_eof758;
case 758:
	if ( (*( p)) == 35 )
		goto st750;
	goto tr237;
tr2316:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1823;
st1823:
	if ( ++( p) == ( pe) )
		goto _test_eof1823;
case 1823:
#line 17843 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2319;
		case 91: goto tr2132;
		case 111: goto tr2319;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2319:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1824;
st1824:
	if ( ++( p) == ( pe) )
		goto _test_eof1824;
case 1824:
#line 17869 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 87: goto tr2320;
		case 91: goto tr2132;
		case 119: goto tr2320;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2320:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1825;
st1825:
	if ( ++( p) == ( pe) )
		goto _test_eof1825;
case 1825:
#line 17895 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2321;
		case 91: goto tr2132;
		case 110: goto tr2321;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2321:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1826;
st1826:
	if ( ++( p) == ( pe) )
		goto _test_eof1826;
case 1826:
#line 17921 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st749;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2310:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1827;
st1827:
	if ( ++( p) == ( pe) )
		goto _test_eof1827;
case 1827:
#line 17946 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2322;
		case 91: goto tr2132;
		case 117: goto tr2322;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2322:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1828;
st1828:
	if ( ++( p) == ( pe) )
		goto _test_eof1828;
case 1828:
#line 17972 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2323;
		case 91: goto tr2132;
		case 109: goto tr2323;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2323:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1829;
st1829:
	if ( ++( p) == ( pe) )
		goto _test_eof1829;
case 1829:
#line 17998 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 66: goto tr2324;
		case 91: goto tr2132;
		case 98: goto tr2324;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2324:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1830;
st1830:
	if ( ++( p) == ( pe) )
		goto _test_eof1830;
case 1830:
#line 18024 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st759;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st759:
	if ( ++( p) == ( pe) )
		goto _test_eof759;
case 759:
	if ( (*( p)) == 35 )
		goto st760;
	goto tr237;
st760:
	if ( ++( p) == ( pe) )
		goto _test_eof760;
case 760:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr924;
	goto tr237;
tr924:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1831;
st1831:
	if ( ++( p) == ( pe) )
		goto _test_eof1831;
case 1831:
#line 18061 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1831;
	goto tr2326;
tr2311:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1832;
st1832:
	if ( ++( p) == ( pe) )
		goto _test_eof1832;
case 1832:
#line 18075 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2328;
		case 91: goto tr2132;
		case 99: goto tr2328;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2328:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1833;
st1833:
	if ( ++( p) == ( pe) )
		goto _test_eof1833;
case 1833:
#line 18101 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2329;
		case 91: goto tr2132;
		case 107: goto tr2329;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2329:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1834;
st1834:
	if ( ++( p) == ( pe) )
		goto _test_eof1834;
case 1834:
#line 18127 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2330;
		case 91: goto tr2132;
		case 101: goto tr2330;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2330:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1835;
st1835:
	if ( ++( p) == ( pe) )
		goto _test_eof1835;
case 1835:
#line 18153 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2331;
		case 91: goto tr2132;
		case 116: goto tr2331;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2331:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1836;
st1836:
	if ( ++( p) == ( pe) )
		goto _test_eof1836;
case 1836:
#line 18179 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st761;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st761:
	if ( ++( p) == ( pe) )
		goto _test_eof761;
case 761:
	if ( (*( p)) == 35 )
		goto st762;
	goto tr237;
st762:
	if ( ++( p) == ( pe) )
		goto _test_eof762;
case 762:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr926;
	goto tr237;
tr926:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1837;
st1837:
	if ( ++( p) == ( pe) )
		goto _test_eof1837;
case 1837:
#line 18216 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1837;
	goto tr2333;
tr2312:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1838;
st1838:
	if ( ++( p) == ( pe) )
		goto _test_eof1838;
case 1838:
#line 18230 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2335;
		case 91: goto tr2132;
		case 112: goto tr2335;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2335:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1839;
st1839:
	if ( ++( p) == ( pe) )
		goto _test_eof1839;
case 1839:
#line 18256 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2336;
		case 91: goto tr2132;
		case 105: goto tr2336;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2336:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1840;
st1840:
	if ( ++( p) == ( pe) )
		goto _test_eof1840;
case 1840:
#line 18282 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2337;
		case 91: goto tr2132;
		case 99: goto tr2337;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2337:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1841;
st1841:
	if ( ++( p) == ( pe) )
		goto _test_eof1841;
case 1841:
#line 18308 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st653;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2095:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1842;
st1842:
	if ( ++( p) == ( pe) )
		goto _test_eof1842;
case 1842:
#line 18335 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2338;
		case 91: goto tr2132;
		case 115: goto tr2338;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2338:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1843;
st1843:
	if ( ++( p) == ( pe) )
		goto _test_eof1843;
case 1843:
#line 18361 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2339;
		case 91: goto tr2132;
		case 101: goto tr2339;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2339:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1844;
st1844:
	if ( ++( p) == ( pe) )
		goto _test_eof1844;
case 1844:
#line 18387 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2340;
		case 91: goto tr2132;
		case 114: goto tr2340;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2340:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1845;
st1845:
	if ( ++( p) == ( pe) )
		goto _test_eof1845;
case 1845:
#line 18413 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st763;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st763:
	if ( ++( p) == ( pe) )
		goto _test_eof763;
case 763:
	if ( (*( p)) == 35 )
		goto st764;
	goto tr237;
st764:
	if ( ++( p) == ( pe) )
		goto _test_eof764;
case 764:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr928;
	goto tr237;
tr928:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1846;
st1846:
	if ( ++( p) == ( pe) )
		goto _test_eof1846;
case 1846:
#line 18450 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1846;
	goto tr2342;
tr2096:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1847;
st1847:
	if ( ++( p) == ( pe) )
		goto _test_eof1847;
case 1847:
#line 18466 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2344;
		case 91: goto tr2132;
		case 105: goto tr2344;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2344:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1848;
st1848:
	if ( ++( p) == ( pe) )
		goto _test_eof1848;
case 1848:
#line 18492 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2345;
		case 91: goto tr2132;
		case 107: goto tr2345;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2345:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1849;
st1849:
	if ( ++( p) == ( pe) )
		goto _test_eof1849;
case 1849:
#line 18518 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2346;
		case 91: goto tr2132;
		case 105: goto tr2346;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2346:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1850;
st1850:
	if ( ++( p) == ( pe) )
		goto _test_eof1850;
case 1850:
#line 18544 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st765;
		case 67: goto tr2348;
		case 80: goto tr2349;
		case 91: goto tr2132;
		case 99: goto tr2348;
		case 112: goto tr2349;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
st765:
	if ( ++( p) == ( pe) )
		goto _test_eof765;
case 765:
	switch( (*( p)) ) {
		case 32: goto st766;
		case 35: goto st767;
		case 67: goto st768;
		case 80: goto st777;
		case 99: goto st768;
		case 112: goto st777;
	}
	goto tr237;
st766:
	if ( ++( p) == ( pe) )
		goto _test_eof766;
case 766:
	if ( (*( p)) == 35 )
		goto st767;
	goto tr237;
st767:
	if ( ++( p) == ( pe) )
		goto _test_eof767;
case 767:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr933;
	goto tr237;
tr933:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1851;
st1851:
	if ( ++( p) == ( pe) )
		goto _test_eof1851;
case 1851:
#line 18598 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1851;
	goto tr2350;
st768:
	if ( ++( p) == ( pe) )
		goto _test_eof768;
case 768:
	switch( (*( p)) ) {
		case 72: goto st769;
		case 104: goto st769;
	}
	goto tr237;
st769:
	if ( ++( p) == ( pe) )
		goto _test_eof769;
case 769:
	switch( (*( p)) ) {
		case 65: goto st770;
		case 97: goto st770;
	}
	goto tr237;
st770:
	if ( ++( p) == ( pe) )
		goto _test_eof770;
case 770:
	switch( (*( p)) ) {
		case 78: goto st771;
		case 110: goto st771;
	}
	goto tr237;
st771:
	if ( ++( p) == ( pe) )
		goto _test_eof771;
case 771:
	switch( (*( p)) ) {
		case 71: goto st772;
		case 103: goto st772;
	}
	goto tr237;
st772:
	if ( ++( p) == ( pe) )
		goto _test_eof772;
case 772:
	switch( (*( p)) ) {
		case 69: goto st773;
		case 101: goto st773;
	}
	goto tr237;
st773:
	if ( ++( p) == ( pe) )
		goto _test_eof773;
case 773:
	switch( (*( p)) ) {
		case 83: goto st774;
		case 115: goto st774;
	}
	goto tr237;
st774:
	if ( ++( p) == ( pe) )
		goto _test_eof774;
case 774:
	if ( (*( p)) == 32 )
		goto st775;
	goto tr237;
st775:
	if ( ++( p) == ( pe) )
		goto _test_eof775;
case 775:
	if ( (*( p)) == 35 )
		goto st776;
	goto tr237;
st776:
	if ( ++( p) == ( pe) )
		goto _test_eof776;
case 776:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr942;
	goto tr237;
tr942:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1852;
st1852:
	if ( ++( p) == ( pe) )
		goto _test_eof1852;
case 1852:
#line 18685 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1852;
	goto tr2352;
st777:
	if ( ++( p) == ( pe) )
		goto _test_eof777;
case 777:
	switch( (*( p)) ) {
		case 65: goto st778;
		case 97: goto st778;
	}
	goto tr237;
st778:
	if ( ++( p) == ( pe) )
		goto _test_eof778;
case 778:
	switch( (*( p)) ) {
		case 71: goto st779;
		case 103: goto st779;
	}
	goto tr237;
st779:
	if ( ++( p) == ( pe) )
		goto _test_eof779;
case 779:
	switch( (*( p)) ) {
		case 69: goto st780;
		case 101: goto st780;
	}
	goto tr237;
st780:
	if ( ++( p) == ( pe) )
		goto _test_eof780;
case 780:
	if ( (*( p)) == 32 )
		goto st781;
	goto tr237;
st781:
	if ( ++( p) == ( pe) )
		goto _test_eof781;
case 781:
	switch( (*( p)) ) {
		case 32: goto st775;
		case 35: goto st782;
		case 67: goto st768;
		case 99: goto st768;
	}
	goto tr237;
st782:
	if ( ++( p) == ( pe) )
		goto _test_eof782;
case 782:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr948;
	goto tr237;
tr948:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1853;
st1853:
	if ( ++( p) == ( pe) )
		goto _test_eof1853;
case 1853:
#line 18749 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1853;
	goto tr2350;
tr2348:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1854;
st1854:
	if ( ++( p) == ( pe) )
		goto _test_eof1854;
case 1854:
#line 18763 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 72: goto tr2355;
		case 91: goto tr2132;
		case 104: goto tr2355;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2355:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1855;
st1855:
	if ( ++( p) == ( pe) )
		goto _test_eof1855;
case 1855:
#line 18789 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2356;
		case 91: goto tr2132;
		case 97: goto tr2356;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2356:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1856;
st1856:
	if ( ++( p) == ( pe) )
		goto _test_eof1856;
case 1856:
#line 18815 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2357;
		case 91: goto tr2132;
		case 110: goto tr2357;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2357:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1857;
st1857:
	if ( ++( p) == ( pe) )
		goto _test_eof1857;
case 1857:
#line 18841 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2358;
		case 91: goto tr2132;
		case 103: goto tr2358;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2358:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1858;
st1858:
	if ( ++( p) == ( pe) )
		goto _test_eof1858;
case 1858:
#line 18867 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2359;
		case 91: goto tr2132;
		case 101: goto tr2359;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2359:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1859;
st1859:
	if ( ++( p) == ( pe) )
		goto _test_eof1859;
case 1859:
#line 18893 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2360;
		case 91: goto tr2132;
		case 115: goto tr2360;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2360:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1860;
st1860:
	if ( ++( p) == ( pe) )
		goto _test_eof1860;
case 1860:
#line 18919 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st775;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2349:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1861;
st1861:
	if ( ++( p) == ( pe) )
		goto _test_eof1861;
case 1861:
#line 18944 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2361;
		case 91: goto tr2132;
		case 97: goto tr2361;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2361:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1862;
st1862:
	if ( ++( p) == ( pe) )
		goto _test_eof1862;
case 1862:
#line 18970 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2362;
		case 91: goto tr2132;
		case 103: goto tr2362;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2362:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1863;
st1863:
	if ( ++( p) == ( pe) )
		goto _test_eof1863;
case 1863:
#line 18996 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2363;
		case 91: goto tr2132;
		case 101: goto tr2363;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2363:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 592 "ext/dtext/dtext.cpp.rl"
	{( act) = 98;}
	goto st1864;
st1864:
	if ( ++( p) == ( pe) )
		goto _test_eof1864;
case 1864:
#line 19022 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st781;
		case 91: goto tr2132;
		case 123: goto tr2133;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2131;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2131;
	} else
		goto tr2131;
	goto tr2103;
tr2097:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1865;
st1865:
	if ( ++( p) == ( pe) )
		goto _test_eof1865;
case 1865:
#line 19051 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2102;
		case 10: goto tr2102;
		case 13: goto tr2102;
		case 35: goto tr2365;
		case 47: goto tr2366;
		case 66: goto tr2367;
		case 67: goto tr2368;
		case 69: goto tr2369;
		case 72: goto tr2370;
		case 73: goto tr2371;
		case 78: goto tr2372;
		case 81: goto tr2373;
		case 83: goto tr2374;
		case 84: goto tr2375;
		case 85: goto tr2376;
		case 91: goto tr2377;
		case 98: goto tr2367;
		case 99: goto tr2368;
		case 101: goto tr2369;
		case 104: goto tr2370;
		case 105: goto tr2371;
		case 110: goto tr2372;
		case 113: goto tr2373;
		case 115: goto tr2374;
		case 116: goto tr2375;
		case 117: goto tr2376;
	}
	goto tr2364;
tr2364:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st783;
st783:
	if ( ++( p) == ( pe) )
		goto _test_eof783;
case 783:
#line 19089 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
	}
	goto st783;
tr950:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st784;
st784:
	if ( ++( p) == ( pe) )
		goto _test_eof784;
case 784:
#line 19105 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st785;
	goto tr241;
st785:
	if ( ++( p) == ( pe) )
		goto _test_eof785;
case 785:
	switch( (*( p)) ) {
		case 35: goto tr952;
		case 47: goto tr952;
		case 72: goto tr953;
		case 104: goto tr953;
	}
	goto tr234;
tr952:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st786;
st786:
	if ( ++( p) == ( pe) )
		goto _test_eof786;
case 786:
#line 19128 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 32: goto tr234;
		case 41: goto tr955;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st786;
tr953:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st787;
st787:
	if ( ++( p) == ( pe) )
		goto _test_eof787;
case 787:
#line 19145 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st788;
		case 116: goto st788;
	}
	goto tr234;
st788:
	if ( ++( p) == ( pe) )
		goto _test_eof788;
case 788:
	switch( (*( p)) ) {
		case 84: goto st789;
		case 116: goto st789;
	}
	goto tr234;
st789:
	if ( ++( p) == ( pe) )
		goto _test_eof789;
case 789:
	switch( (*( p)) ) {
		case 80: goto st790;
		case 112: goto st790;
	}
	goto tr234;
st790:
	if ( ++( p) == ( pe) )
		goto _test_eof790;
case 790:
	switch( (*( p)) ) {
		case 58: goto st791;
		case 83: goto st794;
		case 115: goto st794;
	}
	goto tr234;
st791:
	if ( ++( p) == ( pe) )
		goto _test_eof791;
case 791:
	if ( (*( p)) == 47 )
		goto st792;
	goto tr234;
st792:
	if ( ++( p) == ( pe) )
		goto _test_eof792;
case 792:
	if ( (*( p)) == 47 )
		goto st793;
	goto tr234;
st793:
	if ( ++( p) == ( pe) )
		goto _test_eof793;
case 793:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 32: goto tr234;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st786;
st794:
	if ( ++( p) == ( pe) )
		goto _test_eof794;
case 794:
	if ( (*( p)) == 58 )
		goto st791;
	goto tr234;
tr2365:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st795;
st795:
	if ( ++( p) == ( pe) )
		goto _test_eof795;
case 795:
#line 19221 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr964;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
tr964:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st796;
st796:
	if ( ++( p) == ( pe) )
		goto _test_eof796;
case 796:
#line 19242 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st797;
	goto tr241;
st797:
	if ( ++( p) == ( pe) )
		goto _test_eof797;
case 797:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 35: goto tr967;
		case 47: goto tr967;
		case 72: goto tr968;
		case 104: goto tr968;
	}
	goto tr966;
tr966:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st798;
st798:
	if ( ++( p) == ( pe) )
		goto _test_eof798;
case 798:
#line 19268 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 41: goto tr970;
	}
	goto st798;
tr1240:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st799;
tr967:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st799;
st799:
	if ( ++( p) == ( pe) )
		goto _test_eof799;
case 799:
#line 19290 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto st798;
		case 41: goto tr972;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st798;
	goto st799;
tr968:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st800;
st800:
	if ( ++( p) == ( pe) )
		goto _test_eof800;
case 800:
#line 19311 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 41: goto tr970;
		case 84: goto st801;
		case 116: goto st801;
	}
	goto st798;
st801:
	if ( ++( p) == ( pe) )
		goto _test_eof801;
case 801:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 41: goto tr970;
		case 84: goto st802;
		case 116: goto st802;
	}
	goto st798;
st802:
	if ( ++( p) == ( pe) )
		goto _test_eof802;
case 802:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 41: goto tr970;
		case 80: goto st803;
		case 112: goto st803;
	}
	goto st798;
st803:
	if ( ++( p) == ( pe) )
		goto _test_eof803;
case 803:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 41: goto tr970;
		case 58: goto st804;
		case 83: goto st807;
		case 115: goto st807;
	}
	goto st798;
st804:
	if ( ++( p) == ( pe) )
		goto _test_eof804;
case 804:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 41: goto tr970;
		case 47: goto st805;
	}
	goto st798;
st805:
	if ( ++( p) == ( pe) )
		goto _test_eof805;
case 805:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 41: goto tr970;
		case 47: goto st806;
	}
	goto st798;
st806:
	if ( ++( p) == ( pe) )
		goto _test_eof806;
case 806:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto st798;
		case 41: goto tr980;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st798;
	goto st799;
tr980:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 381 "ext/dtext/dtext.cpp.rl"
	{( act) = 48;}
	goto st1866;
tr1322:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 377 "ext/dtext/dtext.cpp.rl"
	{( act) = 47;}
	goto st1866;
st1866:
	if ( ++( p) == ( pe) )
		goto _test_eof1866;
case 1866:
#line 19417 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 32: goto tr234;
		case 41: goto tr955;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st786;
st807:
	if ( ++( p) == ( pe) )
		goto _test_eof807;
case 807:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
		case 41: goto tr970;
		case 58: goto st804;
	}
	goto st798;
tr2366:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st808;
st808:
	if ( ++( p) == ( pe) )
		goto _test_eof808;
case 808:
#line 19448 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 66: goto st809;
		case 67: goto st811;
		case 69: goto st821;
		case 72: goto st827;
		case 73: goto st828;
		case 78: goto st829;
		case 81: goto st835;
		case 83: goto st840;
		case 84: goto st848;
		case 85: goto st859;
		case 93: goto tr964;
		case 98: goto st809;
		case 99: goto st811;
		case 101: goto st821;
		case 104: goto st827;
		case 105: goto st828;
		case 110: goto st829;
		case 113: goto st835;
		case 115: goto st840;
		case 116: goto st848;
		case 117: goto st859;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st809:
	if ( ++( p) == ( pe) )
		goto _test_eof809;
case 809:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 82: goto st810;
		case 93: goto tr992;
		case 114: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st810:
	if ( ++( p) == ( pe) )
		goto _test_eof810;
case 810:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr241;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st811:
	if ( ++( p) == ( pe) )
		goto _test_eof811;
case 811:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 79: goto st812;
		case 93: goto tr964;
		case 111: goto st812;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st812:
	if ( ++( p) == ( pe) )
		goto _test_eof812;
case 812:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 68: goto st813;
		case 76: goto st814;
		case 93: goto tr964;
		case 100: goto st813;
		case 108: goto st814;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st813:
	if ( ++( p) == ( pe) )
		goto _test_eof813;
case 813:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 69: goto st810;
		case 93: goto tr964;
		case 101: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st814:
	if ( ++( p) == ( pe) )
		goto _test_eof814;
case 814:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 71: goto st815;
		case 79: goto st819;
		case 93: goto tr241;
		case 103: goto st815;
		case 111: goto st819;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st815:
	if ( ++( p) == ( pe) )
		goto _test_eof815;
case 815:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 82: goto st816;
		case 93: goto tr964;
		case 114: goto st816;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st816:
	if ( ++( p) == ( pe) )
		goto _test_eof816;
case 816:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 79: goto st817;
		case 93: goto tr964;
		case 111: goto st817;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st817:
	if ( ++( p) == ( pe) )
		goto _test_eof817;
case 817:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 85: goto st818;
		case 93: goto tr964;
		case 117: goto st818;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st818:
	if ( ++( p) == ( pe) )
		goto _test_eof818;
case 818:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 80: goto st810;
		case 93: goto tr964;
		case 112: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st819:
	if ( ++( p) == ( pe) )
		goto _test_eof819;
case 819:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 82: goto st820;
		case 93: goto tr964;
		case 114: goto st820;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st820:
	if ( ++( p) == ( pe) )
		goto _test_eof820;
case 820:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr1002;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
tr1002:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 459 "ext/dtext/dtext.cpp.rl"
	{( act) = 67;}
	goto st1867;
st1867:
	if ( ++( p) == ( pe) )
		goto _test_eof1867;
case 1867:
#line 19685 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st797;
	goto tr2378;
st821:
	if ( ++( p) == ( pe) )
		goto _test_eof821;
case 821:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 88: goto st822;
		case 93: goto tr964;
		case 120: goto st822;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st822:
	if ( ++( p) == ( pe) )
		goto _test_eof822;
case 822:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 80: goto st823;
		case 93: goto tr964;
		case 112: goto st823;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st823:
	if ( ++( p) == ( pe) )
		goto _test_eof823;
case 823:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 65: goto st824;
		case 93: goto tr964;
		case 97: goto st824;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st824:
	if ( ++( p) == ( pe) )
		goto _test_eof824;
case 824:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 78: goto st825;
		case 93: goto tr964;
		case 110: goto st825;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st825:
	if ( ++( p) == ( pe) )
		goto _test_eof825;
case 825:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 68: goto st826;
		case 93: goto tr964;
		case 100: goto st826;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st826:
	if ( ++( p) == ( pe) )
		goto _test_eof826;
case 826:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_EXPAND)  ) _widec += 256;
	}
	switch( _widec ) {
		case 9: goto st783;
		case 32: goto st783;
		case 3165: goto st1668;
	}
	if ( _widec < 11 ) {
		if ( _widec > -1 ) {
			if ( 1 <= _widec && _widec <= 8 )
				goto st795;
		} else
			goto st795;
	} else if ( _widec > 12 ) {
		if ( _widec > 92 ) {
			if ( 94 <= _widec )
				goto st795;
		} else if ( _widec >= 14 )
			goto st795;
	} else
		goto st783;
	goto tr241;
st827:
	if ( ++( p) == ( pe) )
		goto _test_eof827;
case 827:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 82: goto st810;
		case 93: goto tr964;
		case 114: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st828:
	if ( ++( p) == ( pe) )
		goto _test_eof828;
case 828:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr1008;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st829:
	if ( ++( p) == ( pe) )
		goto _test_eof829;
case 829:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 79: goto st830;
		case 93: goto tr964;
		case 111: goto st830;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st830:
	if ( ++( p) == ( pe) )
		goto _test_eof830;
case 830:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 68: goto st831;
		case 93: goto tr964;
		case 100: goto st831;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st831:
	if ( ++( p) == ( pe) )
		goto _test_eof831;
case 831:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 84: goto st832;
		case 93: goto tr964;
		case 116: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st832:
	if ( ++( p) == ( pe) )
		goto _test_eof832;
case 832:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 69: goto st833;
		case 93: goto tr964;
		case 101: goto st833;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st833:
	if ( ++( p) == ( pe) )
		goto _test_eof833;
case 833:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 88: goto st834;
		case 93: goto tr964;
		case 120: goto st834;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st834:
	if ( ++( p) == ( pe) )
		goto _test_eof834;
case 834:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 84: goto st810;
		case 93: goto tr964;
		case 116: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st835:
	if ( ++( p) == ( pe) )
		goto _test_eof835;
case 835:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 85: goto st836;
		case 93: goto tr964;
		case 117: goto st836;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st836:
	if ( ++( p) == ( pe) )
		goto _test_eof836;
case 836:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 79: goto st837;
		case 93: goto tr964;
		case 111: goto st837;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st837:
	if ( ++( p) == ( pe) )
		goto _test_eof837;
case 837:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 84: goto st838;
		case 93: goto tr964;
		case 116: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st838:
	if ( ++( p) == ( pe) )
		goto _test_eof838;
case 838:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 69: goto st839;
		case 93: goto tr964;
		case 101: goto st839;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st839:
	if ( ++( p) == ( pe) )
		goto _test_eof839;
case 839:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	switch( _widec ) {
		case 9: goto st783;
		case 32: goto st783;
		case 2653: goto st1667;
	}
	if ( _widec < 11 ) {
		if ( _widec > -1 ) {
			if ( 1 <= _widec && _widec <= 8 )
				goto st795;
		} else
			goto st795;
	} else if ( _widec > 12 ) {
		if ( _widec > 92 ) {
			if ( 94 <= _widec )
				goto st795;
		} else if ( _widec >= 14 )
			goto st795;
	} else
		goto st783;
	goto tr241;
st840:
	if ( ++( p) == ( pe) )
		goto _test_eof840;
case 840:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 80: goto st841;
		case 93: goto tr1019;
		case 112: goto st841;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st841:
	if ( ++( p) == ( pe) )
		goto _test_eof841;
case 841:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 79: goto st842;
		case 93: goto tr964;
		case 111: goto st842;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st842:
	if ( ++( p) == ( pe) )
		goto _test_eof842;
case 842:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 73: goto st843;
		case 93: goto tr964;
		case 105: goto st843;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st843:
	if ( ++( p) == ( pe) )
		goto _test_eof843;
case 843:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 76: goto st844;
		case 93: goto tr964;
		case 108: goto st844;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st844:
	if ( ++( p) == ( pe) )
		goto _test_eof844;
case 844:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 69: goto st845;
		case 93: goto tr964;
		case 101: goto st845;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st845:
	if ( ++( p) == ( pe) )
		goto _test_eof845;
case 845:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 82: goto st846;
		case 93: goto tr964;
		case 114: goto st846;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st846:
	if ( ++( p) == ( pe) )
		goto _test_eof846;
case 846:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 83: goto st847;
		case 93: goto tr351;
		case 115: goto st847;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st847:
	if ( ++( p) == ( pe) )
		goto _test_eof847;
case 847:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr351;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st848:
	if ( ++( p) == ( pe) )
		goto _test_eof848;
case 848:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 65: goto st849;
		case 66: goto st851;
		case 68: goto st854;
		case 72: goto st855;
		case 78: goto st858;
		case 82: goto st810;
		case 93: goto tr964;
		case 97: goto st849;
		case 98: goto st851;
		case 100: goto st854;
		case 104: goto st855;
		case 110: goto st858;
		case 114: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st849:
	if ( ++( p) == ( pe) )
		goto _test_eof849;
case 849:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 66: goto st850;
		case 93: goto tr964;
		case 98: goto st850;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st850:
	if ( ++( p) == ( pe) )
		goto _test_eof850;
case 850:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 76: goto st813;
		case 93: goto tr964;
		case 108: goto st813;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st851:
	if ( ++( p) == ( pe) )
		goto _test_eof851;
case 851:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 79: goto st852;
		case 93: goto tr964;
		case 111: goto st852;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st852:
	if ( ++( p) == ( pe) )
		goto _test_eof852;
case 852:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 68: goto st853;
		case 93: goto tr964;
		case 100: goto st853;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st853:
	if ( ++( p) == ( pe) )
		goto _test_eof853;
case 853:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 89: goto st810;
		case 93: goto tr964;
		case 121: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st854:
	if ( ++( p) == ( pe) )
		goto _test_eof854;
case 854:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr278;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st855:
	if ( ++( p) == ( pe) )
		goto _test_eof855;
case 855:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 69: goto st856;
		case 93: goto tr279;
		case 101: goto st856;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st856:
	if ( ++( p) == ( pe) )
		goto _test_eof856;
case 856:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 65: goto st857;
		case 93: goto tr964;
		case 97: goto st857;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st857:
	if ( ++( p) == ( pe) )
		goto _test_eof857;
case 857:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 68: goto st810;
		case 93: goto tr964;
		case 100: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st858:
	if ( ++( p) == ( pe) )
		goto _test_eof858;
case 858:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr330;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st859:
	if ( ++( p) == ( pe) )
		goto _test_eof859;
case 859:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 82: goto st860;
		case 93: goto tr1037;
		case 114: goto st860;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
st860:
	if ( ++( p) == ( pe) )
		goto _test_eof860;
case 860:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 76: goto st810;
		case 93: goto tr964;
		case 108: goto st810;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st795;
tr2367:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st861;
st861:
	if ( ++( p) == ( pe) )
		goto _test_eof861;
case 861:
#line 20369 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st862;
		case 93: goto tr1039;
		case 114: goto st862;
	}
	goto st783;
st862:
	if ( ++( p) == ( pe) )
		goto _test_eof862;
case 862:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1040;
	}
	goto st783;
tr2368:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st863;
st863:
	if ( ++( p) == ( pe) )
		goto _test_eof863;
case 863:
#line 20398 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st864;
		case 93: goto tr950;
		case 111: goto st864;
	}
	goto st783;
st864:
	if ( ++( p) == ( pe) )
		goto _test_eof864;
case 864:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 68: goto st865;
		case 76: goto st872;
		case 93: goto tr950;
		case 100: goto st865;
		case 108: goto st872;
	}
	goto st783;
st865:
	if ( ++( p) == ( pe) )
		goto _test_eof865;
case 865:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st866;
		case 93: goto tr950;
		case 101: goto st866;
	}
	goto st783;
st866:
	if ( ++( p) == ( pe) )
		goto _test_eof866;
case 866:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st867;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st867;
		case 61: goto st868;
		case 93: goto tr1047;
	}
	goto st783;
st867:
	if ( ++( p) == ( pe) )
		goto _test_eof867;
case 867:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st867;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st867;
		case 61: goto st868;
		case 93: goto tr950;
	}
	goto st783;
st868:
	if ( ++( p) == ( pe) )
		goto _test_eof868;
case 868:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st868;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st868;
		case 93: goto tr950;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1048;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1048;
	} else
		goto tr1048;
	goto st783;
tr1048:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st869;
st869:
	if ( ++( p) == ( pe) )
		goto _test_eof869;
case 869:
#line 20493 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1050;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st869;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st869;
	} else
		goto st869;
	goto st783;
tr1050:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 470 "ext/dtext/dtext.cpp.rl"
	{( act) = 69;}
	goto st1868;
st1868:
	if ( ++( p) == ( pe) )
		goto _test_eof1868;
case 1868:
#line 20523 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1052;
		case 9: goto st870;
		case 10: goto tr1052;
		case 32: goto st870;
		case 40: goto st785;
	}
	goto tr2379;
st870:
	if ( ++( p) == ( pe) )
		goto _test_eof870;
case 870:
	switch( (*( p)) ) {
		case 0: goto tr1052;
		case 9: goto st870;
		case 10: goto tr1052;
		case 32: goto st870;
	}
	goto tr1051;
tr1047:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1869;
st1869:
	if ( ++( p) == ( pe) )
		goto _test_eof1869;
case 1869:
#line 20551 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1055;
		case 9: goto st871;
		case 10: goto tr1055;
		case 32: goto st871;
	}
	goto tr2380;
st871:
	if ( ++( p) == ( pe) )
		goto _test_eof871;
case 871:
	switch( (*( p)) ) {
		case 0: goto tr1055;
		case 9: goto st871;
		case 10: goto tr1055;
		case 32: goto st871;
	}
	goto tr1054;
st872:
	if ( ++( p) == ( pe) )
		goto _test_eof872;
case 872:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 71: goto st873;
		case 79: goto st878;
		case 93: goto tr241;
		case 103: goto st873;
		case 111: goto st878;
	}
	goto st783;
st873:
	if ( ++( p) == ( pe) )
		goto _test_eof873;
case 873:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st874;
		case 93: goto tr950;
		case 114: goto st874;
	}
	goto st783;
st874:
	if ( ++( p) == ( pe) )
		goto _test_eof874;
case 874:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st875;
		case 93: goto tr950;
		case 111: goto st875;
	}
	goto st783;
st875:
	if ( ++( p) == ( pe) )
		goto _test_eof875;
case 875:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 85: goto st876;
		case 93: goto tr950;
		case 117: goto st876;
	}
	goto st783;
st876:
	if ( ++( p) == ( pe) )
		goto _test_eof876;
case 876:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st877;
		case 93: goto tr950;
		case 112: goto st877;
	}
	goto st783;
st877:
	if ( ++( p) == ( pe) )
		goto _test_eof877;
case 877:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr241;
	}
	goto st783;
st878:
	if ( ++( p) == ( pe) )
		goto _test_eof878;
case 878:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st879;
		case 93: goto tr950;
		case 114: goto st879;
	}
	goto st783;
st879:
	if ( ++( p) == ( pe) )
		goto _test_eof879;
case 879:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 61: goto st880;
		case 93: goto tr950;
	}
	goto st783;
st880:
	if ( ++( p) == ( pe) )
		goto _test_eof880;
case 880:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 35: goto tr1065;
		case 65: goto tr1066;
		case 67: goto tr1067;
		case 69: goto tr1068;
		case 71: goto tr1069;
		case 73: goto tr1070;
		case 76: goto tr1071;
		case 77: goto tr1072;
		case 79: goto tr1073;
		case 81: goto tr1074;
		case 83: goto tr1075;
		case 86: goto tr1076;
		case 93: goto tr950;
		case 97: goto tr1077;
		case 99: goto tr1079;
		case 101: goto tr1080;
		case 103: goto tr1081;
		case 105: goto tr1082;
		case 108: goto tr1083;
		case 109: goto tr1084;
		case 111: goto tr1085;
		case 113: goto tr1086;
		case 115: goto tr1087;
		case 118: goto tr1088;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto tr1078;
	goto st783;
tr1065:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st881;
st881:
	if ( ++( p) == ( pe) )
		goto _test_eof881;
case 881:
#line 20717 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st882;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st882;
	} else
		goto st882;
	goto st783;
st882:
	if ( ++( p) == ( pe) )
		goto _test_eof882;
case 882:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st883;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st883;
	} else
		goto st883;
	goto st783;
st883:
	if ( ++( p) == ( pe) )
		goto _test_eof883;
case 883:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st884;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st884;
	} else
		goto st884;
	goto st783;
st884:
	if ( ++( p) == ( pe) )
		goto _test_eof884;
case 884:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1093;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st885;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st885;
	} else
		goto st885;
	goto st783;
st885:
	if ( ++( p) == ( pe) )
		goto _test_eof885;
case 885:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1093;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st886;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st886;
	} else
		goto st886;
	goto st783;
st886:
	if ( ++( p) == ( pe) )
		goto _test_eof886;
case 886:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1093;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st887;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st887;
	} else
		goto st887;
	goto st783;
st887:
	if ( ++( p) == ( pe) )
		goto _test_eof887;
case 887:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1093;
	}
	goto st783;
tr1093:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 446 "ext/dtext/dtext.cpp.rl"
	{( act) = 66;}
	goto st1870;
tr1099:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 438 "ext/dtext/dtext.cpp.rl"
	{( act) = 65;}
	goto st1870;
st1870:
	if ( ++( p) == ( pe) )
		goto _test_eof1870;
case 1870:
#line 20863 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st785;
	goto tr234;
tr1066:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st888;
st888:
	if ( ++( p) == ( pe) )
		goto _test_eof888;
case 888:
#line 20875 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st889;
		case 93: goto tr950;
		case 114: goto st889;
	}
	goto st783;
st889:
	if ( ++( p) == ( pe) )
		goto _test_eof889;
case 889:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st890;
		case 93: goto tr950;
		case 116: goto st890;
	}
	goto st783;
st890:
	if ( ++( p) == ( pe) )
		goto _test_eof890;
case 890:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st891;
		case 93: goto tr1099;
		case 105: goto st891;
	}
	goto st783;
st891:
	if ( ++( p) == ( pe) )
		goto _test_eof891;
case 891:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 83: goto st892;
		case 93: goto tr950;
		case 115: goto st892;
	}
	goto st783;
st892:
	if ( ++( p) == ( pe) )
		goto _test_eof892;
case 892:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st893;
		case 93: goto tr950;
		case 116: goto st893;
	}
	goto st783;
st893:
	if ( ++( p) == ( pe) )
		goto _test_eof893;
case 893:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1099;
	}
	goto st783;
tr1067:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st894;
st894:
	if ( ++( p) == ( pe) )
		goto _test_eof894;
case 894:
#line 20956 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 72: goto st895;
		case 79: goto st902;
		case 93: goto tr950;
		case 104: goto st895;
		case 111: goto st902;
	}
	goto st783;
st895:
	if ( ++( p) == ( pe) )
		goto _test_eof895;
case 895:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st896;
		case 93: goto tr1099;
		case 97: goto st896;
	}
	goto st783;
st896:
	if ( ++( p) == ( pe) )
		goto _test_eof896;
case 896:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st897;
		case 93: goto tr950;
		case 114: goto st897;
	}
	goto st783;
st897:
	if ( ++( p) == ( pe) )
		goto _test_eof897;
case 897:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st898;
		case 93: goto tr1099;
		case 97: goto st898;
	}
	goto st783;
st898:
	if ( ++( p) == ( pe) )
		goto _test_eof898;
case 898:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st899;
		case 93: goto tr950;
		case 99: goto st899;
	}
	goto st783;
st899:
	if ( ++( p) == ( pe) )
		goto _test_eof899;
case 899:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st900;
		case 93: goto tr950;
		case 116: goto st900;
	}
	goto st783;
st900:
	if ( ++( p) == ( pe) )
		goto _test_eof900;
case 900:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st901;
		case 93: goto tr950;
		case 101: goto st901;
	}
	goto st783;
st901:
	if ( ++( p) == ( pe) )
		goto _test_eof901;
case 901:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st893;
		case 93: goto tr950;
		case 114: goto st893;
	}
	goto st783;
st902:
	if ( ++( p) == ( pe) )
		goto _test_eof902;
case 902:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st903;
		case 93: goto tr1099;
		case 112: goto st903;
	}
	goto st783;
st903:
	if ( ++( p) == ( pe) )
		goto _test_eof903;
case 903:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 89: goto st904;
		case 93: goto tr950;
		case 121: goto st904;
	}
	goto st783;
st904:
	if ( ++( p) == ( pe) )
		goto _test_eof904;
case 904:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st905;
		case 93: goto tr1099;
		case 114: goto st905;
	}
	goto st783;
st905:
	if ( ++( p) == ( pe) )
		goto _test_eof905;
case 905:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st906;
		case 93: goto tr950;
		case 105: goto st906;
	}
	goto st783;
st906:
	if ( ++( p) == ( pe) )
		goto _test_eof906;
case 906:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 71: goto st907;
		case 93: goto tr950;
		case 103: goto st907;
	}
	goto st783;
st907:
	if ( ++( p) == ( pe) )
		goto _test_eof907;
case 907:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 72: goto st892;
		case 93: goto tr950;
		case 104: goto st892;
	}
	goto st783;
tr1068:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st908;
st908:
	if ( ++( p) == ( pe) )
		goto _test_eof908;
case 908:
#line 21145 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 88: goto st909;
		case 93: goto tr1099;
		case 120: goto st909;
	}
	goto st783;
st909:
	if ( ++( p) == ( pe) )
		goto _test_eof909;
case 909:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st910;
		case 93: goto tr950;
		case 112: goto st910;
	}
	goto st783;
st910:
	if ( ++( p) == ( pe) )
		goto _test_eof910;
case 910:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st911;
		case 93: goto tr950;
		case 108: goto st911;
	}
	goto st783;
st911:
	if ( ++( p) == ( pe) )
		goto _test_eof911;
case 911:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st912;
		case 93: goto tr950;
		case 105: goto st912;
	}
	goto st783;
st912:
	if ( ++( p) == ( pe) )
		goto _test_eof912;
case 912:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st913;
		case 93: goto tr950;
		case 99: goto st913;
	}
	goto st783;
st913:
	if ( ++( p) == ( pe) )
		goto _test_eof913;
case 913:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st892;
		case 93: goto tr950;
		case 105: goto st892;
	}
	goto st783;
tr1069:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st914;
st914:
	if ( ++( p) == ( pe) )
		goto _test_eof914;
case 914:
#line 21228 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st915;
		case 93: goto tr950;
		case 101: goto st915;
	}
	goto st783;
st915:
	if ( ++( p) == ( pe) )
		goto _test_eof915;
case 915:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 78: goto st916;
		case 93: goto tr950;
		case 110: goto st916;
	}
	goto st783;
st916:
	if ( ++( p) == ( pe) )
		goto _test_eof916;
case 916:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st917;
		case 93: goto tr1099;
		case 101: goto st917;
	}
	goto st783;
st917:
	if ( ++( p) == ( pe) )
		goto _test_eof917;
case 917:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st918;
		case 93: goto tr950;
		case 114: goto st918;
	}
	goto st783;
st918:
	if ( ++( p) == ( pe) )
		goto _test_eof918;
case 918:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st919;
		case 93: goto tr950;
		case 97: goto st919;
	}
	goto st783;
st919:
	if ( ++( p) == ( pe) )
		goto _test_eof919;
case 919:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st893;
		case 93: goto tr950;
		case 108: goto st893;
	}
	goto st783;
tr1070:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st920;
st920:
	if ( ++( p) == ( pe) )
		goto _test_eof920;
case 920:
#line 21311 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 78: goto st921;
		case 93: goto tr950;
		case 110: goto st921;
	}
	goto st783;
st921:
	if ( ++( p) == ( pe) )
		goto _test_eof921;
case 921:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 86: goto st922;
		case 93: goto tr950;
		case 118: goto st922;
	}
	goto st783;
st922:
	if ( ++( p) == ( pe) )
		goto _test_eof922;
case 922:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st923;
		case 93: goto tr1099;
		case 97: goto st923;
	}
	goto st783;
st923:
	if ( ++( p) == ( pe) )
		goto _test_eof923;
case 923:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st924;
		case 93: goto tr950;
		case 108: goto st924;
	}
	goto st783;
st924:
	if ( ++( p) == ( pe) )
		goto _test_eof924;
case 924:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st925;
		case 93: goto tr950;
		case 105: goto st925;
	}
	goto st783;
st925:
	if ( ++( p) == ( pe) )
		goto _test_eof925;
case 925:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 68: goto st893;
		case 93: goto tr950;
		case 100: goto st893;
	}
	goto st783;
tr1071:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st926;
st926:
	if ( ++( p) == ( pe) )
		goto _test_eof926;
case 926:
#line 21394 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st927;
		case 93: goto tr950;
		case 111: goto st927;
	}
	goto st783;
st927:
	if ( ++( p) == ( pe) )
		goto _test_eof927;
case 927:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st928;
		case 93: goto tr950;
		case 114: goto st928;
	}
	goto st783;
st928:
	if ( ++( p) == ( pe) )
		goto _test_eof928;
case 928:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st893;
		case 93: goto tr1099;
		case 101: goto st893;
	}
	goto st783;
tr1072:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st929;
st929:
	if ( ++( p) == ( pe) )
		goto _test_eof929;
case 929:
#line 21438 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st930;
		case 93: goto tr950;
		case 101: goto st930;
	}
	goto st783;
st930:
	if ( ++( p) == ( pe) )
		goto _test_eof930;
case 930:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st931;
		case 93: goto tr950;
		case 116: goto st931;
	}
	goto st783;
st931:
	if ( ++( p) == ( pe) )
		goto _test_eof931;
case 931:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st893;
		case 93: goto tr950;
		case 97: goto st893;
	}
	goto st783;
tr1073:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st932;
st932:
	if ( ++( p) == ( pe) )
		goto _test_eof932;
case 932:
#line 21482 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st893;
		case 93: goto tr950;
		case 99: goto st893;
	}
	goto st783;
tr1074:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st933;
st933:
	if ( ++( p) == ( pe) )
		goto _test_eof933;
case 933:
#line 21500 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 85: goto st934;
		case 93: goto tr1099;
		case 117: goto st934;
	}
	goto st783;
st934:
	if ( ++( p) == ( pe) )
		goto _test_eof934;
case 934:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st935;
		case 93: goto tr950;
		case 101: goto st935;
	}
	goto st783;
st935:
	if ( ++( p) == ( pe) )
		goto _test_eof935;
case 935:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 83: goto st936;
		case 93: goto tr950;
		case 115: goto st936;
	}
	goto st783;
st936:
	if ( ++( p) == ( pe) )
		goto _test_eof936;
case 936:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st937;
		case 93: goto tr950;
		case 116: goto st937;
	}
	goto st783;
st937:
	if ( ++( p) == ( pe) )
		goto _test_eof937;
case 937:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st938;
		case 93: goto tr950;
		case 105: goto st938;
	}
	goto st783;
st938:
	if ( ++( p) == ( pe) )
		goto _test_eof938;
case 938:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st939;
		case 93: goto tr950;
		case 111: goto st939;
	}
	goto st783;
st939:
	if ( ++( p) == ( pe) )
		goto _test_eof939;
case 939:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 78: goto st940;
		case 93: goto tr950;
		case 110: goto st940;
	}
	goto st783;
st940:
	if ( ++( p) == ( pe) )
		goto _test_eof940;
case 940:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st941;
		case 93: goto tr950;
		case 97: goto st941;
	}
	goto st783;
st941:
	if ( ++( p) == ( pe) )
		goto _test_eof941;
case 941:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 66: goto st942;
		case 93: goto tr950;
		case 98: goto st942;
	}
	goto st783;
st942:
	if ( ++( p) == ( pe) )
		goto _test_eof942;
case 942:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st943;
		case 93: goto tr950;
		case 108: goto st943;
	}
	goto st783;
st943:
	if ( ++( p) == ( pe) )
		goto _test_eof943;
case 943:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st893;
		case 93: goto tr950;
		case 101: goto st893;
	}
	goto st783;
tr1075:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st944;
st944:
	if ( ++( p) == ( pe) )
		goto _test_eof944;
case 944:
#line 21648 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st945;
		case 80: goto st946;
		case 93: goto tr1099;
		case 97: goto st945;
		case 112: goto st946;
	}
	goto st783;
st945:
	if ( ++( p) == ( pe) )
		goto _test_eof945;
case 945:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 70: goto st943;
		case 93: goto tr950;
		case 102: goto st943;
	}
	goto st783;
st946:
	if ( ++( p) == ( pe) )
		goto _test_eof946;
case 946:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st947;
		case 93: goto tr950;
		case 101: goto st947;
	}
	goto st783;
st947:
	if ( ++( p) == ( pe) )
		goto _test_eof947;
case 947:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st948;
		case 93: goto tr950;
		case 99: goto st948;
	}
	goto st783;
st948:
	if ( ++( p) == ( pe) )
		goto _test_eof948;
case 948:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st949;
		case 93: goto tr1099;
		case 105: goto st949;
	}
	goto st783;
st949:
	if ( ++( p) == ( pe) )
		goto _test_eof949;
case 949:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st950;
		case 93: goto tr950;
		case 101: goto st950;
	}
	goto st783;
st950:
	if ( ++( p) == ( pe) )
		goto _test_eof950;
case 950:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 83: goto st893;
		case 93: goto tr950;
		case 115: goto st893;
	}
	goto st783;
tr1076:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st951;
st951:
	if ( ++( p) == ( pe) )
		goto _test_eof951;
case 951:
#line 21746 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st952;
		case 93: goto tr950;
		case 111: goto st952;
	}
	goto st783;
st952:
	if ( ++( p) == ( pe) )
		goto _test_eof952;
case 952:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st953;
		case 93: goto tr950;
		case 105: goto st953;
	}
	goto st783;
st953:
	if ( ++( p) == ( pe) )
		goto _test_eof953;
case 953:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st954;
		case 93: goto tr950;
		case 99: goto st954;
	}
	goto st783;
st954:
	if ( ++( p) == ( pe) )
		goto _test_eof954;
case 954:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st955;
		case 93: goto tr950;
		case 101: goto st955;
	}
	goto st783;
st955:
	if ( ++( p) == ( pe) )
		goto _test_eof955;
case 955:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 45: goto st956;
		case 93: goto tr950;
	}
	goto st783;
st956:
	if ( ++( p) == ( pe) )
		goto _test_eof956;
case 956:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st957;
		case 93: goto tr950;
		case 97: goto st957;
	}
	goto st783;
st957:
	if ( ++( p) == ( pe) )
		goto _test_eof957;
case 957:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st958;
		case 93: goto tr950;
		case 99: goto st958;
	}
	goto st783;
st958:
	if ( ++( p) == ( pe) )
		goto _test_eof958;
case 958:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st959;
		case 93: goto tr950;
		case 116: goto st959;
	}
	goto st783;
st959:
	if ( ++( p) == ( pe) )
		goto _test_eof959;
case 959:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st901;
		case 93: goto tr950;
		case 111: goto st901;
	}
	goto st783;
tr1077:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st960;
st960:
	if ( ++( p) == ( pe) )
		goto _test_eof960;
case 960:
#line 21867 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st889;
		case 93: goto tr1093;
		case 114: goto st962;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1078:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st961;
st961:
	if ( ++( p) == ( pe) )
		goto _test_eof961;
case 961:
#line 21887 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1093;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st962:
	if ( ++( p) == ( pe) )
		goto _test_eof962;
case 962:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st890;
		case 93: goto tr1093;
		case 116: goto st963;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st963:
	if ( ++( p) == ( pe) )
		goto _test_eof963;
case 963:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st891;
		case 93: goto tr1099;
		case 105: goto st964;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st964:
	if ( ++( p) == ( pe) )
		goto _test_eof964;
case 964:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 83: goto st892;
		case 93: goto tr1093;
		case 115: goto st965;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st965:
	if ( ++( p) == ( pe) )
		goto _test_eof965;
case 965:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st893;
		case 93: goto tr1093;
		case 116: goto st966;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st966:
	if ( ++( p) == ( pe) )
		goto _test_eof966;
case 966:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1099;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1079:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st967;
st967:
	if ( ++( p) == ( pe) )
		goto _test_eof967;
case 967:
#line 21978 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 72: goto st895;
		case 79: goto st902;
		case 93: goto tr1093;
		case 104: goto st968;
		case 111: goto st975;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st968:
	if ( ++( p) == ( pe) )
		goto _test_eof968;
case 968:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st896;
		case 93: goto tr1099;
		case 97: goto st969;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st969:
	if ( ++( p) == ( pe) )
		goto _test_eof969;
case 969:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st897;
		case 93: goto tr1093;
		case 114: goto st970;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st970:
	if ( ++( p) == ( pe) )
		goto _test_eof970;
case 970:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st898;
		case 93: goto tr1099;
		case 97: goto st971;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st971:
	if ( ++( p) == ( pe) )
		goto _test_eof971;
case 971:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st899;
		case 93: goto tr1093;
		case 99: goto st972;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st972:
	if ( ++( p) == ( pe) )
		goto _test_eof972;
case 972:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st900;
		case 93: goto tr1093;
		case 116: goto st973;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st973:
	if ( ++( p) == ( pe) )
		goto _test_eof973;
case 973:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st901;
		case 93: goto tr1093;
		case 101: goto st974;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st974:
	if ( ++( p) == ( pe) )
		goto _test_eof974;
case 974:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st893;
		case 93: goto tr1093;
		case 114: goto st966;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st975:
	if ( ++( p) == ( pe) )
		goto _test_eof975;
case 975:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st903;
		case 93: goto tr1099;
		case 112: goto st976;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st976:
	if ( ++( p) == ( pe) )
		goto _test_eof976;
case 976:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 89: goto st904;
		case 93: goto tr1093;
		case 121: goto st977;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st977:
	if ( ++( p) == ( pe) )
		goto _test_eof977;
case 977:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st905;
		case 93: goto tr1099;
		case 114: goto st978;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st978:
	if ( ++( p) == ( pe) )
		goto _test_eof978;
case 978:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st906;
		case 93: goto tr1093;
		case 105: goto st979;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st979:
	if ( ++( p) == ( pe) )
		goto _test_eof979;
case 979:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 71: goto st907;
		case 93: goto tr1093;
		case 103: goto st980;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st980:
	if ( ++( p) == ( pe) )
		goto _test_eof980;
case 980:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 72: goto st892;
		case 93: goto tr1093;
		case 104: goto st965;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1080:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st981;
st981:
	if ( ++( p) == ( pe) )
		goto _test_eof981;
case 981:
#line 22195 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 88: goto st909;
		case 93: goto tr1099;
		case 120: goto st982;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st982:
	if ( ++( p) == ( pe) )
		goto _test_eof982;
case 982:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st910;
		case 93: goto tr1093;
		case 112: goto st983;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st983:
	if ( ++( p) == ( pe) )
		goto _test_eof983;
case 983:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st911;
		case 93: goto tr1093;
		case 108: goto st984;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st984:
	if ( ++( p) == ( pe) )
		goto _test_eof984;
case 984:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st912;
		case 93: goto tr1093;
		case 105: goto st985;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st985:
	if ( ++( p) == ( pe) )
		goto _test_eof985;
case 985:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st913;
		case 93: goto tr1093;
		case 99: goto st986;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st986:
	if ( ++( p) == ( pe) )
		goto _test_eof986;
case 986:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st892;
		case 93: goto tr1093;
		case 105: goto st965;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1081:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st987;
st987:
	if ( ++( p) == ( pe) )
		goto _test_eof987;
case 987:
#line 22290 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st915;
		case 93: goto tr1093;
		case 101: goto st988;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st988:
	if ( ++( p) == ( pe) )
		goto _test_eof988;
case 988:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 78: goto st916;
		case 93: goto tr1093;
		case 110: goto st989;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st989:
	if ( ++( p) == ( pe) )
		goto _test_eof989;
case 989:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st917;
		case 93: goto tr1099;
		case 101: goto st990;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st990:
	if ( ++( p) == ( pe) )
		goto _test_eof990;
case 990:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st918;
		case 93: goto tr1093;
		case 114: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st991:
	if ( ++( p) == ( pe) )
		goto _test_eof991;
case 991:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st919;
		case 93: goto tr1093;
		case 97: goto st992;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st992:
	if ( ++( p) == ( pe) )
		goto _test_eof992;
case 992:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st893;
		case 93: goto tr1093;
		case 108: goto st966;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1082:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st993;
st993:
	if ( ++( p) == ( pe) )
		goto _test_eof993;
case 993:
#line 22385 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 78: goto st921;
		case 93: goto tr1093;
		case 110: goto st994;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st994:
	if ( ++( p) == ( pe) )
		goto _test_eof994;
case 994:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 86: goto st922;
		case 93: goto tr1093;
		case 118: goto st995;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st995:
	if ( ++( p) == ( pe) )
		goto _test_eof995;
case 995:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st923;
		case 93: goto tr1099;
		case 97: goto st996;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st996:
	if ( ++( p) == ( pe) )
		goto _test_eof996;
case 996:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st924;
		case 93: goto tr1093;
		case 108: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st997:
	if ( ++( p) == ( pe) )
		goto _test_eof997;
case 997:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st925;
		case 93: goto tr1093;
		case 105: goto st998;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st998:
	if ( ++( p) == ( pe) )
		goto _test_eof998;
case 998:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 68: goto st893;
		case 93: goto tr1093;
		case 100: goto st966;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1083:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st999;
st999:
	if ( ++( p) == ( pe) )
		goto _test_eof999;
case 999:
#line 22480 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st927;
		case 93: goto tr1093;
		case 111: goto st1000;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1000:
	if ( ++( p) == ( pe) )
		goto _test_eof1000;
case 1000:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st928;
		case 93: goto tr1093;
		case 114: goto st1001;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1001:
	if ( ++( p) == ( pe) )
		goto _test_eof1001;
case 1001:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st893;
		case 93: goto tr1099;
		case 101: goto st966;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1084:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1002;
st1002:
	if ( ++( p) == ( pe) )
		goto _test_eof1002;
case 1002:
#line 22530 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st930;
		case 93: goto tr1093;
		case 101: goto st1003;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1003:
	if ( ++( p) == ( pe) )
		goto _test_eof1003;
case 1003:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st931;
		case 93: goto tr1093;
		case 116: goto st1004;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1004:
	if ( ++( p) == ( pe) )
		goto _test_eof1004;
case 1004:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st893;
		case 93: goto tr1093;
		case 97: goto st966;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1085:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1005;
st1005:
	if ( ++( p) == ( pe) )
		goto _test_eof1005;
case 1005:
#line 22580 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st893;
		case 93: goto tr1093;
		case 99: goto st966;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1086:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1006;
st1006:
	if ( ++( p) == ( pe) )
		goto _test_eof1006;
case 1006:
#line 22600 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 85: goto st934;
		case 93: goto tr1099;
		case 117: goto st1007;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1007:
	if ( ++( p) == ( pe) )
		goto _test_eof1007;
case 1007:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st935;
		case 93: goto tr1093;
		case 101: goto st1008;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1008:
	if ( ++( p) == ( pe) )
		goto _test_eof1008;
case 1008:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 83: goto st936;
		case 93: goto tr1093;
		case 115: goto st1009;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1009:
	if ( ++( p) == ( pe) )
		goto _test_eof1009;
case 1009:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st937;
		case 93: goto tr1093;
		case 116: goto st1010;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1010:
	if ( ++( p) == ( pe) )
		goto _test_eof1010;
case 1010:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st938;
		case 93: goto tr1093;
		case 105: goto st1011;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1011:
	if ( ++( p) == ( pe) )
		goto _test_eof1011;
case 1011:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st939;
		case 93: goto tr1093;
		case 111: goto st1012;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1012:
	if ( ++( p) == ( pe) )
		goto _test_eof1012;
case 1012:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 78: goto st940;
		case 93: goto tr1093;
		case 110: goto st1013;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1013:
	if ( ++( p) == ( pe) )
		goto _test_eof1013;
case 1013:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st941;
		case 93: goto tr1093;
		case 97: goto st1014;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1014:
	if ( ++( p) == ( pe) )
		goto _test_eof1014;
case 1014:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 66: goto st942;
		case 93: goto tr1093;
		case 98: goto st1015;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1015:
	if ( ++( p) == ( pe) )
		goto _test_eof1015;
case 1015:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st943;
		case 93: goto tr1093;
		case 108: goto st1016;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1016:
	if ( ++( p) == ( pe) )
		goto _test_eof1016;
case 1016:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st893;
		case 93: goto tr1093;
		case 101: goto st966;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1087:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1017;
st1017:
	if ( ++( p) == ( pe) )
		goto _test_eof1017;
case 1017:
#line 22770 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st945;
		case 80: goto st946;
		case 93: goto tr1099;
		case 97: goto st1018;
		case 112: goto st1019;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1018:
	if ( ++( p) == ( pe) )
		goto _test_eof1018;
case 1018:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 70: goto st943;
		case 93: goto tr1093;
		case 102: goto st1016;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1019:
	if ( ++( p) == ( pe) )
		goto _test_eof1019;
case 1019:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st947;
		case 93: goto tr1093;
		case 101: goto st1020;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1020:
	if ( ++( p) == ( pe) )
		goto _test_eof1020;
case 1020:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st948;
		case 93: goto tr1093;
		case 99: goto st1021;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1021:
	if ( ++( p) == ( pe) )
		goto _test_eof1021;
case 1021:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st949;
		case 93: goto tr1099;
		case 105: goto st1022;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1022:
	if ( ++( p) == ( pe) )
		goto _test_eof1022;
case 1022:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st950;
		case 93: goto tr1093;
		case 101: goto st1023;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1023:
	if ( ++( p) == ( pe) )
		goto _test_eof1023;
case 1023:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 83: goto st893;
		case 93: goto tr1093;
		case 115: goto st966;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr1088:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1024;
st1024:
	if ( ++( p) == ( pe) )
		goto _test_eof1024;
case 1024:
#line 22882 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st952;
		case 93: goto tr1093;
		case 97: goto st966;
		case 111: goto st1025;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1025:
	if ( ++( p) == ( pe) )
		goto _test_eof1025;
case 1025:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st953;
		case 93: goto tr1093;
		case 105: goto st1026;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1026:
	if ( ++( p) == ( pe) )
		goto _test_eof1026;
case 1026:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 67: goto st954;
		case 93: goto tr1093;
		case 99: goto st1027;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1027:
	if ( ++( p) == ( pe) )
		goto _test_eof1027;
case 1027:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st955;
		case 93: goto tr1093;
		case 101: goto st1028;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
st1028:
	if ( ++( p) == ( pe) )
		goto _test_eof1028;
case 1028:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 45: goto st956;
		case 93: goto tr1093;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st961;
	goto st783;
tr2369:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1029;
st1029:
	if ( ++( p) == ( pe) )
		goto _test_eof1029;
case 1029:
#line 22962 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 88: goto st1030;
		case 93: goto tr950;
		case 120: goto st1030;
	}
	goto st783;
st1030:
	if ( ++( p) == ( pe) )
		goto _test_eof1030;
case 1030:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st1031;
		case 93: goto tr950;
		case 112: goto st1031;
	}
	goto st783;
st1031:
	if ( ++( p) == ( pe) )
		goto _test_eof1031;
case 1031:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st1032;
		case 93: goto tr950;
		case 97: goto st1032;
	}
	goto st783;
st1032:
	if ( ++( p) == ( pe) )
		goto _test_eof1032;
case 1032:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 78: goto st1033;
		case 93: goto tr950;
		case 110: goto st1033;
	}
	goto st783;
st1033:
	if ( ++( p) == ( pe) )
		goto _test_eof1033;
case 1033:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 68: goto st877;
		case 93: goto tr950;
		case 100: goto st877;
	}
	goto st783;
tr2370:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1034;
st1034:
	if ( ++( p) == ( pe) )
		goto _test_eof1034;
case 1034:
#line 23034 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st877;
		case 84: goto st1035;
		case 93: goto tr950;
		case 114: goto st877;
		case 116: goto st1035;
	}
	goto st783;
st1035:
	if ( ++( p) == ( pe) )
		goto _test_eof1035;
case 1035:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1036;
		case 93: goto tr950;
		case 116: goto st1036;
	}
	goto st783;
st1036:
	if ( ++( p) == ( pe) )
		goto _test_eof1036;
case 1036:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st1037;
		case 93: goto tr950;
		case 112: goto st1037;
	}
	goto st783;
st1037:
	if ( ++( p) == ( pe) )
		goto _test_eof1037;
case 1037:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 58: goto st1038;
		case 83: goto st1059;
		case 93: goto tr950;
		case 115: goto st1059;
	}
	goto st783;
st1038:
	if ( ++( p) == ( pe) )
		goto _test_eof1038;
case 1038:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1039;
		case 93: goto tr950;
	}
	goto st783;
st1039:
	if ( ++( p) == ( pe) )
		goto _test_eof1039;
case 1039:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1040;
		case 93: goto tr950;
	}
	goto st783;
st1040:
	if ( ++( p) == ( pe) )
		goto _test_eof1040;
case 1040:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr1228;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1041;
st1041:
	if ( ++( p) == ( pe) )
		goto _test_eof1041;
case 1041:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr1229;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1041;
tr1229:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1042;
st1042:
	if ( ++( p) == ( pe) )
		goto _test_eof1042;
case 1042:
#line 23148 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 40: goto st797;
		case 93: goto tr1231;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1043:
	if ( ++( p) == ( pe) )
		goto _test_eof1043;
case 1043:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 32: goto tr234;
		case 93: goto tr1231;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st1043;
tr1231:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1044;
st1044:
	if ( ++( p) == ( pe) )
		goto _test_eof1044;
case 1044:
#line 23178 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 32: goto tr234;
		case 40: goto st1045;
		case 93: goto tr1231;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st1043;
st1045:
	if ( ++( p) == ( pe) )
		goto _test_eof1045;
case 1045:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 10: goto tr234;
		case 13: goto tr234;
	}
	goto tr966;
tr1228:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1046;
st1046:
	if ( ++( p) == ( pe) )
		goto _test_eof1046;
case 1046:
#line 23206 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 40: goto st1047;
		case 93: goto tr1231;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1047:
	if ( ++( p) == ( pe) )
		goto _test_eof1047;
case 1047:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 35: goto tr1234;
		case 47: goto tr1234;
		case 72: goto tr1235;
		case 93: goto tr1231;
		case 104: goto tr1235;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
tr1234:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1048;
st1048:
	if ( ++( p) == ( pe) )
		goto _test_eof1048;
case 1048:
#line 23240 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 41: goto tr1237;
		case 93: goto tr1238;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1048;
tr1237:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 385 "ext/dtext/dtext.cpp.rl"
	{( act) = 49;}
	goto st1871;
st1871:
	if ( ++( p) == ( pe) )
		goto _test_eof1871;
case 1871:
#line 23262 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2381;
		case 32: goto tr2381;
		case 93: goto tr1231;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2381;
	goto st1043;
tr1238:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1049;
st1049:
	if ( ++( p) == ( pe) )
		goto _test_eof1049;
case 1049:
#line 23279 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 40: goto st1050;
		case 41: goto tr1237;
		case 93: goto tr1238;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1048;
st1050:
	if ( ++( p) == ( pe) )
		goto _test_eof1050;
case 1050:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr966;
		case 41: goto tr1241;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto tr966;
	goto tr1240;
tr1241:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 385 "ext/dtext/dtext.cpp.rl"
	{( act) = 49;}
	goto st1872;
st1872:
	if ( ++( p) == ( pe) )
		goto _test_eof1872;
case 1872:
#line 23318 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2381;
		case 10: goto tr2381;
		case 13: goto tr2381;
		case 41: goto tr970;
	}
	goto st798;
tr1235:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1051;
st1051:
	if ( ++( p) == ( pe) )
		goto _test_eof1051;
case 1051:
#line 23334 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 84: goto st1052;
		case 93: goto tr1231;
		case 116: goto st1052;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1052:
	if ( ++( p) == ( pe) )
		goto _test_eof1052;
case 1052:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 84: goto st1053;
		case 93: goto tr1231;
		case 116: goto st1053;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1053:
	if ( ++( p) == ( pe) )
		goto _test_eof1053;
case 1053:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 80: goto st1054;
		case 93: goto tr1231;
		case 112: goto st1054;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1054:
	if ( ++( p) == ( pe) )
		goto _test_eof1054;
case 1054:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 58: goto st1055;
		case 83: goto st1058;
		case 93: goto tr1231;
		case 115: goto st1058;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1055:
	if ( ++( p) == ( pe) )
		goto _test_eof1055;
case 1055:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 47: goto st1056;
		case 93: goto tr1231;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1056:
	if ( ++( p) == ( pe) )
		goto _test_eof1056;
case 1056:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 47: goto st1057;
		case 93: goto tr1231;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1057:
	if ( ++( p) == ( pe) )
		goto _test_eof1057;
case 1057:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 93: goto tr1238;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1048;
st1058:
	if ( ++( p) == ( pe) )
		goto _test_eof1058;
case 1058:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 58: goto st1055;
		case 93: goto tr1231;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1043;
st1059:
	if ( ++( p) == ( pe) )
		goto _test_eof1059;
case 1059:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 58: goto st1038;
		case 93: goto tr950;
	}
	goto st783;
tr2371:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1060;
st1060:
	if ( ++( p) == ( pe) )
		goto _test_eof1060;
case 1060:
#line 23459 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1249;
	}
	goto st783;
tr2372:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1061;
st1061:
	if ( ++( p) == ( pe) )
		goto _test_eof1061;
case 1061:
#line 23475 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st1062;
		case 93: goto tr950;
		case 111: goto st1062;
	}
	goto st783;
st1062:
	if ( ++( p) == ( pe) )
		goto _test_eof1062;
case 1062:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 68: goto st1063;
		case 93: goto tr950;
		case 100: goto st1063;
	}
	goto st783;
st1063:
	if ( ++( p) == ( pe) )
		goto _test_eof1063;
case 1063:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1064;
		case 93: goto tr950;
		case 116: goto st1064;
	}
	goto st783;
st1064:
	if ( ++( p) == ( pe) )
		goto _test_eof1064;
case 1064:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st1065;
		case 93: goto tr950;
		case 101: goto st1065;
	}
	goto st783;
st1065:
	if ( ++( p) == ( pe) )
		goto _test_eof1065;
case 1065:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 88: goto st1066;
		case 93: goto tr950;
		case 120: goto st1066;
	}
	goto st783;
st1066:
	if ( ++( p) == ( pe) )
		goto _test_eof1066;
case 1066:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1067;
		case 93: goto tr950;
		case 116: goto st1067;
	}
	goto st783;
st1067:
	if ( ++( p) == ( pe) )
		goto _test_eof1067;
case 1067:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1256;
	}
	goto st783;
tr1256:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1873;
st1873:
	if ( ++( p) == ( pe) )
		goto _test_eof1873;
case 1873:
#line 23569 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1258;
		case 9: goto st1068;
		case 10: goto tr1258;
		case 32: goto st1068;
	}
	goto tr2382;
st1068:
	if ( ++( p) == ( pe) )
		goto _test_eof1068;
case 1068:
	switch( (*( p)) ) {
		case 0: goto tr1258;
		case 9: goto st1068;
		case 10: goto tr1258;
		case 32: goto st1068;
	}
	goto tr1257;
tr2373:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1069;
st1069:
	if ( ++( p) == ( pe) )
		goto _test_eof1069;
case 1069:
#line 23596 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 85: goto st1070;
		case 93: goto tr950;
		case 117: goto st1070;
	}
	goto st783;
st1070:
	if ( ++( p) == ( pe) )
		goto _test_eof1070;
case 1070:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st1071;
		case 93: goto tr950;
		case 111: goto st1071;
	}
	goto st783;
st1071:
	if ( ++( p) == ( pe) )
		goto _test_eof1071;
case 1071:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1072;
		case 93: goto tr950;
		case 116: goto st1072;
	}
	goto st783;
st1072:
	if ( ++( p) == ( pe) )
		goto _test_eof1072;
case 1072:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st1073;
		case 93: goto tr950;
		case 101: goto st1073;
	}
	goto st783;
st1073:
	if ( ++( p) == ( pe) )
		goto _test_eof1073;
case 1073:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr288;
	}
	goto st783;
tr2374:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1074;
st1074:
	if ( ++( p) == ( pe) )
		goto _test_eof1074;
case 1074:
#line 23664 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st1075;
		case 93: goto tr1265;
		case 112: goto st1075;
	}
	goto st783;
st1075:
	if ( ++( p) == ( pe) )
		goto _test_eof1075;
case 1075:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st1076;
		case 93: goto tr950;
		case 111: goto st1076;
	}
	goto st783;
st1076:
	if ( ++( p) == ( pe) )
		goto _test_eof1076;
case 1076:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 73: goto st1077;
		case 93: goto tr950;
		case 105: goto st1077;
	}
	goto st783;
st1077:
	if ( ++( p) == ( pe) )
		goto _test_eof1077;
case 1077:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st1078;
		case 93: goto tr950;
		case 108: goto st1078;
	}
	goto st783;
st1078:
	if ( ++( p) == ( pe) )
		goto _test_eof1078;
case 1078:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st1079;
		case 93: goto tr950;
		case 101: goto st1079;
	}
	goto st783;
st1079:
	if ( ++( p) == ( pe) )
		goto _test_eof1079;
case 1079:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st1080;
		case 93: goto tr950;
		case 114: goto st1080;
	}
	goto st783;
st1080:
	if ( ++( p) == ( pe) )
		goto _test_eof1080;
case 1080:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 83: goto st1081;
		case 93: goto tr1272;
		case 115: goto st1081;
	}
	goto st783;
st1081:
	if ( ++( p) == ( pe) )
		goto _test_eof1081;
case 1081:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1272;
	}
	goto st783;
tr2375:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1082;
st1082:
	if ( ++( p) == ( pe) )
		goto _test_eof1082;
case 1082:
#line 23771 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st1083;
		case 66: goto st1086;
		case 68: goto st877;
		case 72: goto st1089;
		case 78: goto st1091;
		case 82: goto st877;
		case 93: goto tr950;
		case 97: goto st1083;
		case 98: goto st1086;
		case 100: goto st877;
		case 104: goto st1089;
		case 110: goto st1091;
		case 114: goto st877;
	}
	goto st783;
st1083:
	if ( ++( p) == ( pe) )
		goto _test_eof1083;
case 1083:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 66: goto st1084;
		case 93: goto tr950;
		case 98: goto st1084;
	}
	goto st783;
st1084:
	if ( ++( p) == ( pe) )
		goto _test_eof1084;
case 1084:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st1085;
		case 93: goto tr950;
		case 108: goto st1085;
	}
	goto st783;
st1085:
	if ( ++( p) == ( pe) )
		goto _test_eof1085;
case 1085:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st877;
		case 93: goto tr950;
		case 101: goto st877;
	}
	goto st783;
st1086:
	if ( ++( p) == ( pe) )
		goto _test_eof1086;
case 1086:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 79: goto st1087;
		case 93: goto tr950;
		case 111: goto st1087;
	}
	goto st783;
st1087:
	if ( ++( p) == ( pe) )
		goto _test_eof1087;
case 1087:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 68: goto st1088;
		case 93: goto tr950;
		case 100: goto st1088;
	}
	goto st783;
st1088:
	if ( ++( p) == ( pe) )
		goto _test_eof1088;
case 1088:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 89: goto st877;
		case 93: goto tr950;
		case 121: goto st877;
	}
	goto st783;
st1089:
	if ( ++( p) == ( pe) )
		goto _test_eof1089;
case 1089:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 69: goto st1090;
		case 93: goto tr241;
		case 101: goto st1090;
	}
	goto st783;
st1090:
	if ( ++( p) == ( pe) )
		goto _test_eof1090;
case 1090:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 65: goto st1033;
		case 93: goto tr950;
		case 97: goto st1033;
	}
	goto st783;
st1091:
	if ( ++( p) == ( pe) )
		goto _test_eof1091;
case 1091:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr1282;
	}
	goto st783;
tr2376:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1092;
st1092:
	if ( ++( p) == ( pe) )
		goto _test_eof1092;
case 1092:
#line 23914 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 82: goto st1093;
		case 93: goto tr1284;
		case 114: goto st1093;
	}
	goto st783;
st1093:
	if ( ++( p) == ( pe) )
		goto _test_eof1093;
case 1093:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 76: goto st1094;
		case 93: goto tr950;
		case 108: goto st1094;
	}
	goto st783;
st1094:
	if ( ++( p) == ( pe) )
		goto _test_eof1094;
case 1094:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1095;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1095;
		case 61: goto st1096;
		case 93: goto st1190;
	}
	goto st783;
st1095:
	if ( ++( p) == ( pe) )
		goto _test_eof1095;
case 1095:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1095;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1095;
		case 61: goto st1096;
		case 93: goto tr950;
	}
	goto st783;
st1096:
	if ( ++( p) == ( pe) )
		goto _test_eof1096;
case 1096:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1096;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1096;
		case 34: goto st1097;
		case 35: goto tr1290;
		case 39: goto st1148;
		case 47: goto tr1290;
		case 72: goto tr1292;
		case 93: goto tr950;
		case 104: goto tr1292;
	}
	goto st783;
st1097:
	if ( ++( p) == ( pe) )
		goto _test_eof1097;
case 1097:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 35: goto tr1293;
		case 47: goto tr1293;
		case 72: goto tr1294;
		case 93: goto tr950;
		case 104: goto tr1294;
	}
	goto st783;
tr1293:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1098;
st1098:
	if ( ++( p) == ( pe) )
		goto _test_eof1098;
case 1098:
#line 24007 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 34: goto tr1296;
		case 93: goto tr1297;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1098;
tr1296:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1099;
st1099:
	if ( ++( p) == ( pe) )
		goto _test_eof1099;
case 1099:
#line 24027 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1099;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1099;
		case 93: goto tr1299;
	}
	goto st783;
tr1299:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1100;
tr1357:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1100;
st1100:
	if ( ++( p) == ( pe) )
		goto _test_eof1100;
case 1100:
#line 24051 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1108;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1108;
		case 40: goto tr1302;
	}
	goto tr1300;
tr1300:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1101;
st1101:
	if ( ++( p) == ( pe) )
		goto _test_eof1101;
case 1101:
#line 24069 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr1304;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr1304;
		case 91: goto tr1305;
	}
	goto st1101;
tr1304:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1102;
st1102:
	if ( ++( p) == ( pe) )
		goto _test_eof1102;
case 1102:
#line 24087 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto st1102;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto st1102;
		case 91: goto st1103;
	}
	goto st1101;
tr1305:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1103;
st1103:
	if ( ++( p) == ( pe) )
		goto _test_eof1103;
case 1103:
#line 24105 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr1304;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr1304;
		case 47: goto st1104;
		case 91: goto tr1305;
	}
	goto st1101;
st1104:
	if ( ++( p) == ( pe) )
		goto _test_eof1104;
case 1104:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr1304;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr1304;
		case 85: goto st1105;
		case 91: goto tr1305;
		case 117: goto st1105;
	}
	goto st1101;
st1105:
	if ( ++( p) == ( pe) )
		goto _test_eof1105;
case 1105:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr1304;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr1304;
		case 82: goto st1106;
		case 91: goto tr1305;
		case 114: goto st1106;
	}
	goto st1101;
st1106:
	if ( ++( p) == ( pe) )
		goto _test_eof1106;
case 1106:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr1304;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr1304;
		case 76: goto st1107;
		case 91: goto tr1305;
		case 108: goto st1107;
	}
	goto st1101;
st1107:
	if ( ++( p) == ( pe) )
		goto _test_eof1107;
case 1107:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr1304;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto tr1304;
		case 91: goto tr1305;
		case 93: goto tr1312;
	}
	goto st1101;
tr1392:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1108;
st1108:
	if ( ++( p) == ( pe) )
		goto _test_eof1108;
case 1108:
#line 24183 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto st1108;
		case 10: goto tr234;
		case 13: goto tr234;
		case 32: goto st1108;
	}
	goto tr1300;
tr1302:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1109;
st1109:
	if ( ++( p) == ( pe) )
		goto _test_eof1109;
case 1109:
#line 24200 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 35: goto tr1313;
		case 47: goto tr1313;
		case 72: goto tr1314;
		case 91: goto tr1305;
		case 104: goto tr1314;
	}
	goto st1101;
tr1340:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1110;
tr1313:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1110;
st1110:
	if ( ++( p) == ( pe) )
		goto _test_eof1110;
case 1110:
#line 24226 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 41: goto tr1316;
		case 91: goto tr1317;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1101;
	goto st1110;
tr1316:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 385 "ext/dtext/dtext.cpp.rl"
	{( act) = 49;}
	goto st1874;
tr1341:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 385 "ext/dtext/dtext.cpp.rl"
	{( act) = 49;}
	goto st1874;
st1874:
	if ( ++( p) == ( pe) )
		goto _test_eof1874;
case 1874:
#line 24261 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2381;
		case 9: goto tr1304;
		case 10: goto tr2381;
		case 13: goto tr2381;
		case 32: goto tr1304;
		case 91: goto tr1305;
	}
	goto st1101;
tr1317:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1111;
st1111:
	if ( ++( p) == ( pe) )
		goto _test_eof1111;
case 1111:
#line 24279 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 41: goto tr1316;
		case 47: goto st1112;
		case 91: goto tr1317;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1101;
	goto st1110;
st1112:
	if ( ++( p) == ( pe) )
		goto _test_eof1112;
case 1112:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 41: goto tr1316;
		case 85: goto st1113;
		case 91: goto tr1317;
		case 117: goto st1113;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1101;
	goto st1110;
st1113:
	if ( ++( p) == ( pe) )
		goto _test_eof1113;
case 1113:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 41: goto tr1316;
		case 82: goto st1114;
		case 91: goto tr1317;
		case 114: goto st1114;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1101;
	goto st1110;
st1114:
	if ( ++( p) == ( pe) )
		goto _test_eof1114;
case 1114:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 41: goto tr1316;
		case 76: goto st1115;
		case 91: goto tr1317;
		case 108: goto st1115;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1101;
	goto st1110;
st1115:
	if ( ++( p) == ( pe) )
		goto _test_eof1115;
case 1115:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 41: goto tr1316;
		case 91: goto tr1317;
		case 93: goto tr1322;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1101;
	goto st1110;
tr1314:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1116;
st1116:
	if ( ++( p) == ( pe) )
		goto _test_eof1116;
case 1116:
#line 24372 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 84: goto st1117;
		case 91: goto tr1305;
		case 116: goto st1117;
	}
	goto st1101;
st1117:
	if ( ++( p) == ( pe) )
		goto _test_eof1117;
case 1117:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 84: goto st1118;
		case 91: goto tr1305;
		case 116: goto st1118;
	}
	goto st1101;
st1118:
	if ( ++( p) == ( pe) )
		goto _test_eof1118;
case 1118:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 80: goto st1119;
		case 91: goto tr1305;
		case 112: goto st1119;
	}
	goto st1101;
st1119:
	if ( ++( p) == ( pe) )
		goto _test_eof1119;
case 1119:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 58: goto st1120;
		case 83: goto st1123;
		case 91: goto tr1305;
		case 115: goto st1123;
	}
	goto st1101;
st1120:
	if ( ++( p) == ( pe) )
		goto _test_eof1120;
case 1120:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 47: goto st1121;
		case 91: goto tr1305;
	}
	goto st1101;
st1121:
	if ( ++( p) == ( pe) )
		goto _test_eof1121;
case 1121:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 47: goto st1122;
		case 91: goto tr1305;
	}
	goto st1101;
st1122:
	if ( ++( p) == ( pe) )
		goto _test_eof1122;
case 1122:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 91: goto tr1317;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1101;
	goto st1110;
st1123:
	if ( ++( p) == ( pe) )
		goto _test_eof1123;
case 1123:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1304;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1304;
		case 58: goto st1120;
		case 91: goto tr1305;
	}
	goto st1101;
tr1297:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1124;
st1124:
	if ( ++( p) == ( pe) )
		goto _test_eof1124;
case 1124:
#line 24495 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 40: goto st1127;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
st1125:
	if ( ++( p) == ( pe) )
		goto _test_eof1125;
case 1125:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 32: goto tr234;
		case 34: goto tr1331;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st1125;
tr1331:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1126;
st1126:
	if ( ++( p) == ( pe) )
		goto _test_eof1126;
case 1126:
#line 24525 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1126;
		case 32: goto st1126;
		case 93: goto st1108;
	}
	goto tr234;
st1127:
	if ( ++( p) == ( pe) )
		goto _test_eof1127;
case 1127:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 35: goto tr1334;
		case 47: goto tr1334;
		case 72: goto tr1335;
		case 104: goto tr1335;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
tr1334:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1128;
st1128:
	if ( ++( p) == ( pe) )
		goto _test_eof1128;
case 1128:
#line 24556 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1337;
		case 41: goto tr1338;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1128;
tr1337:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1129;
st1129:
	if ( ++( p) == ( pe) )
		goto _test_eof1129;
case 1129:
#line 24574 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1126;
		case 32: goto st1126;
		case 41: goto tr955;
		case 93: goto st1130;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st786;
tr1397:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1130;
st1130:
	if ( ++( p) == ( pe) )
		goto _test_eof1130;
case 1130:
#line 24593 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1108;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1108;
		case 41: goto tr1341;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr1300;
	goto tr1340;
tr1338:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 385 "ext/dtext/dtext.cpp.rl"
	{( act) = 49;}
	goto st1875;
st1875:
	if ( ++( p) == ( pe) )
		goto _test_eof1875;
case 1875:
#line 24617 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2381;
		case 32: goto tr2381;
		case 34: goto tr1331;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2381;
	goto st1125;
tr1335:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1131;
st1131:
	if ( ++( p) == ( pe) )
		goto _test_eof1131;
case 1131:
#line 24634 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 84: goto st1132;
		case 116: goto st1132;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
st1132:
	if ( ++( p) == ( pe) )
		goto _test_eof1132;
case 1132:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 84: goto st1133;
		case 116: goto st1133;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
st1133:
	if ( ++( p) == ( pe) )
		goto _test_eof1133;
case 1133:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 80: goto st1134;
		case 112: goto st1134;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
st1134:
	if ( ++( p) == ( pe) )
		goto _test_eof1134;
case 1134:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 58: goto st1135;
		case 83: goto st1138;
		case 115: goto st1138;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
st1135:
	if ( ++( p) == ( pe) )
		goto _test_eof1135;
case 1135:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 47: goto st1136;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
st1136:
	if ( ++( p) == ( pe) )
		goto _test_eof1136;
case 1136:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 47: goto st1137;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
st1137:
	if ( ++( p) == ( pe) )
		goto _test_eof1137;
case 1137:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1337;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1128;
st1138:
	if ( ++( p) == ( pe) )
		goto _test_eof1138;
case 1138:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1331;
		case 58: goto st1135;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1125;
tr1294:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1139;
st1139:
	if ( ++( p) == ( pe) )
		goto _test_eof1139;
case 1139:
#line 24747 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1140;
		case 93: goto tr950;
		case 116: goto st1140;
	}
	goto st783;
st1140:
	if ( ++( p) == ( pe) )
		goto _test_eof1140;
case 1140:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1141;
		case 93: goto tr950;
		case 116: goto st1141;
	}
	goto st783;
st1141:
	if ( ++( p) == ( pe) )
		goto _test_eof1141;
case 1141:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st1142;
		case 93: goto tr950;
		case 112: goto st1142;
	}
	goto st783;
st1142:
	if ( ++( p) == ( pe) )
		goto _test_eof1142;
case 1142:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 58: goto st1143;
		case 83: goto st1146;
		case 93: goto tr950;
		case 115: goto st1146;
	}
	goto st783;
st1143:
	if ( ++( p) == ( pe) )
		goto _test_eof1143;
case 1143:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1144;
		case 93: goto tr950;
	}
	goto st783;
st1144:
	if ( ++( p) == ( pe) )
		goto _test_eof1144;
case 1144:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1145;
		case 93: goto tr950;
	}
	goto st783;
st1145:
	if ( ++( p) == ( pe) )
		goto _test_eof1145;
case 1145:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr1297;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1098;
st1146:
	if ( ++( p) == ( pe) )
		goto _test_eof1146;
case 1146:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 58: goto st1143;
		case 93: goto tr950;
	}
	goto st783;
tr1290:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1147;
st1147:
	if ( ++( p) == ( pe) )
		goto _test_eof1147;
case 1147:
#line 24855 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1296;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1296;
		case 93: goto tr1357;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1147;
st1148:
	if ( ++( p) == ( pe) )
		goto _test_eof1148;
case 1148:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 35: goto tr1358;
		case 47: goto tr1358;
		case 72: goto tr1359;
		case 93: goto tr950;
		case 104: goto tr1359;
	}
	goto st783;
tr1358:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1149;
st1149:
	if ( ++( p) == ( pe) )
		goto _test_eof1149;
case 1149:
#line 24890 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 39: goto tr1296;
		case 93: goto tr1361;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1149;
tr1361:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1150;
st1150:
	if ( ++( p) == ( pe) )
		goto _test_eof1150;
case 1150:
#line 24910 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1331;
		case 40: goto st1152;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
st1151:
	if ( ++( p) == ( pe) )
		goto _test_eof1151;
case 1151:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 32: goto tr234;
		case 39: goto tr1331;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st1151;
st1152:
	if ( ++( p) == ( pe) )
		goto _test_eof1152;
case 1152:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 35: goto tr1364;
		case 39: goto tr1331;
		case 47: goto tr1364;
		case 72: goto tr1365;
		case 104: goto tr1365;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
tr1364:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1153;
st1153:
	if ( ++( p) == ( pe) )
		goto _test_eof1153;
case 1153:
#line 24956 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1337;
		case 41: goto tr1367;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1153;
tr1367:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 385 "ext/dtext/dtext.cpp.rl"
	{( act) = 49;}
	goto st1876;
st1876:
	if ( ++( p) == ( pe) )
		goto _test_eof1876;
case 1876:
#line 24978 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2381;
		case 32: goto tr2381;
		case 39: goto tr1331;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2381;
	goto st1151;
tr1365:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1154;
st1154:
	if ( ++( p) == ( pe) )
		goto _test_eof1154;
case 1154:
#line 24995 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1331;
		case 84: goto st1155;
		case 116: goto st1155;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
st1155:
	if ( ++( p) == ( pe) )
		goto _test_eof1155;
case 1155:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1331;
		case 84: goto st1156;
		case 116: goto st1156;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
st1156:
	if ( ++( p) == ( pe) )
		goto _test_eof1156;
case 1156:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1331;
		case 80: goto st1157;
		case 112: goto st1157;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
st1157:
	if ( ++( p) == ( pe) )
		goto _test_eof1157;
case 1157:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1331;
		case 58: goto st1158;
		case 83: goto st1161;
		case 115: goto st1161;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
st1158:
	if ( ++( p) == ( pe) )
		goto _test_eof1158;
case 1158:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1331;
		case 47: goto st1159;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
st1159:
	if ( ++( p) == ( pe) )
		goto _test_eof1159;
case 1159:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1331;
		case 47: goto st1160;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
st1160:
	if ( ++( p) == ( pe) )
		goto _test_eof1160;
case 1160:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1337;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1153;
st1161:
	if ( ++( p) == ( pe) )
		goto _test_eof1161;
case 1161:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 39: goto tr1331;
		case 58: goto st1158;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1151;
tr1359:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1162;
st1162:
	if ( ++( p) == ( pe) )
		goto _test_eof1162;
case 1162:
#line 25108 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1163;
		case 93: goto tr950;
		case 116: goto st1163;
	}
	goto st783;
st1163:
	if ( ++( p) == ( pe) )
		goto _test_eof1163;
case 1163:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1164;
		case 93: goto tr950;
		case 116: goto st1164;
	}
	goto st783;
st1164:
	if ( ++( p) == ( pe) )
		goto _test_eof1164;
case 1164:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st1165;
		case 93: goto tr950;
		case 112: goto st1165;
	}
	goto st783;
st1165:
	if ( ++( p) == ( pe) )
		goto _test_eof1165;
case 1165:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 58: goto st1166;
		case 83: goto st1169;
		case 93: goto tr950;
		case 115: goto st1169;
	}
	goto st783;
st1166:
	if ( ++( p) == ( pe) )
		goto _test_eof1166;
case 1166:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1167;
		case 93: goto tr950;
	}
	goto st783;
st1167:
	if ( ++( p) == ( pe) )
		goto _test_eof1167;
case 1167:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1168;
		case 93: goto tr950;
	}
	goto st783;
st1168:
	if ( ++( p) == ( pe) )
		goto _test_eof1168;
case 1168:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr1361;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1149;
st1169:
	if ( ++( p) == ( pe) )
		goto _test_eof1169;
case 1169:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 58: goto st1166;
		case 93: goto tr950;
	}
	goto st783;
tr1292:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1170;
st1170:
	if ( ++( p) == ( pe) )
		goto _test_eof1170;
case 1170:
#line 25216 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1171;
		case 93: goto tr950;
		case 116: goto st1171;
	}
	goto st783;
st1171:
	if ( ++( p) == ( pe) )
		goto _test_eof1171;
case 1171:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 84: goto st1172;
		case 93: goto tr950;
		case 116: goto st1172;
	}
	goto st783;
st1172:
	if ( ++( p) == ( pe) )
		goto _test_eof1172;
case 1172:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 80: goto st1173;
		case 93: goto tr950;
		case 112: goto st1173;
	}
	goto st783;
st1173:
	if ( ++( p) == ( pe) )
		goto _test_eof1173;
case 1173:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 58: goto st1174;
		case 83: goto st1189;
		case 93: goto tr950;
		case 115: goto st1189;
	}
	goto st783;
st1174:
	if ( ++( p) == ( pe) )
		goto _test_eof1174;
case 1174:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1175;
		case 93: goto tr950;
	}
	goto st783;
st1175:
	if ( ++( p) == ( pe) )
		goto _test_eof1175;
case 1175:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1176;
		case 93: goto tr950;
	}
	goto st783;
st1176:
	if ( ++( p) == ( pe) )
		goto _test_eof1176;
case 1176:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st783;
		case 93: goto tr1389;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1147;
tr1389:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1177;
st1177:
	if ( ++( p) == ( pe) )
		goto _test_eof1177;
case 1177:
#line 25312 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 40: goto st1179;
		case 93: goto tr1392;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
st1178:
	if ( ++( p) == ( pe) )
		goto _test_eof1178;
case 1178:
	switch( (*( p)) ) {
		case 0: goto tr234;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 93: goto tr1392;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr234;
	goto st1178;
st1179:
	if ( ++( p) == ( pe) )
		goto _test_eof1179;
case 1179:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 35: goto tr1393;
		case 47: goto tr1393;
		case 72: goto tr1394;
		case 93: goto tr1392;
		case 104: goto tr1394;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
tr1393:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1180;
st1180:
	if ( ++( p) == ( pe) )
		goto _test_eof1180;
case 1180:
#line 25361 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 41: goto tr1396;
		case 93: goto tr1397;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1180;
tr1396:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 385 "ext/dtext/dtext.cpp.rl"
	{( act) = 49;}
	goto st1877;
st1877:
	if ( ++( p) == ( pe) )
		goto _test_eof1877;
case 1877:
#line 25384 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2381;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 93: goto tr1392;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr2381;
	goto st1178;
tr1394:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1181;
st1181:
	if ( ++( p) == ( pe) )
		goto _test_eof1181;
case 1181:
#line 25402 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 84: goto st1182;
		case 93: goto tr1392;
		case 116: goto st1182;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
st1182:
	if ( ++( p) == ( pe) )
		goto _test_eof1182;
case 1182:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 84: goto st1183;
		case 93: goto tr1392;
		case 116: goto st1183;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
st1183:
	if ( ++( p) == ( pe) )
		goto _test_eof1183;
case 1183:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 80: goto st1184;
		case 93: goto tr1392;
		case 112: goto st1184;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
st1184:
	if ( ++( p) == ( pe) )
		goto _test_eof1184;
case 1184:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 58: goto st1185;
		case 83: goto st1188;
		case 93: goto tr1392;
		case 115: goto st1188;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
st1185:
	if ( ++( p) == ( pe) )
		goto _test_eof1185;
case 1185:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 47: goto st1186;
		case 93: goto tr1392;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
st1186:
	if ( ++( p) == ( pe) )
		goto _test_eof1186;
case 1186:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 47: goto st1187;
		case 93: goto tr1392;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
st1187:
	if ( ++( p) == ( pe) )
		goto _test_eof1187;
case 1187:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 93: goto tr1397;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1180;
st1188:
	if ( ++( p) == ( pe) )
		goto _test_eof1188;
case 1188:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1331;
		case 32: goto tr1331;
		case 58: goto st1185;
		case 93: goto tr1392;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1178;
st1189:
	if ( ++( p) == ( pe) )
		goto _test_eof1189;
case 1189:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 58: goto st1174;
		case 93: goto tr950;
	}
	goto st783;
st1190:
	if ( ++( p) == ( pe) )
		goto _test_eof1190;
case 1190:
	switch( (*( p)) ) {
		case 9: goto st1190;
		case 32: goto st1190;
		case 35: goto tr1405;
		case 47: goto tr1405;
		case 72: goto tr1406;
		case 104: goto tr1406;
	}
	goto tr241;
tr1405:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1191;
st1191:
	if ( ++( p) == ( pe) )
		goto _test_eof1191;
case 1191:
#line 25548 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1408;
		case 32: goto tr1408;
		case 91: goto tr1409;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1191;
tr1408:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1192;
st1192:
	if ( ++( p) == ( pe) )
		goto _test_eof1192;
case 1192:
#line 25566 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1192;
		case 32: goto st1192;
		case 91: goto st1193;
	}
	goto tr241;
st1193:
	if ( ++( p) == ( pe) )
		goto _test_eof1193;
case 1193:
	if ( (*( p)) == 47 )
		goto st1194;
	goto tr241;
st1194:
	if ( ++( p) == ( pe) )
		goto _test_eof1194;
case 1194:
	switch( (*( p)) ) {
		case 85: goto st1195;
		case 117: goto st1195;
	}
	goto tr241;
st1195:
	if ( ++( p) == ( pe) )
		goto _test_eof1195;
case 1195:
	switch( (*( p)) ) {
		case 82: goto st1196;
		case 114: goto st1196;
	}
	goto tr241;
st1196:
	if ( ++( p) == ( pe) )
		goto _test_eof1196;
case 1196:
	switch( (*( p)) ) {
		case 76: goto st1197;
		case 108: goto st1197;
	}
	goto tr241;
st1197:
	if ( ++( p) == ( pe) )
		goto _test_eof1197;
case 1197:
	if ( (*( p)) == 93 )
		goto tr1416;
	goto tr241;
tr1409:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1198;
st1198:
	if ( ++( p) == ( pe) )
		goto _test_eof1198;
case 1198:
#line 25622 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1408;
		case 32: goto tr1408;
		case 47: goto st1199;
		case 91: goto tr1409;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1191;
st1199:
	if ( ++( p) == ( pe) )
		goto _test_eof1199;
case 1199:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1408;
		case 32: goto tr1408;
		case 85: goto st1200;
		case 91: goto tr1409;
		case 117: goto st1200;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1191;
st1200:
	if ( ++( p) == ( pe) )
		goto _test_eof1200;
case 1200:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1408;
		case 32: goto tr1408;
		case 82: goto st1201;
		case 91: goto tr1409;
		case 114: goto st1201;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1191;
st1201:
	if ( ++( p) == ( pe) )
		goto _test_eof1201;
case 1201:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1408;
		case 32: goto tr1408;
		case 76: goto st1202;
		case 91: goto tr1409;
		case 108: goto st1202;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1191;
st1202:
	if ( ++( p) == ( pe) )
		goto _test_eof1202;
case 1202:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1408;
		case 32: goto tr1408;
		case 91: goto tr1409;
		case 93: goto tr1416;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1191;
tr1406:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1203;
st1203:
	if ( ++( p) == ( pe) )
		goto _test_eof1203;
case 1203:
#line 25700 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1204;
		case 116: goto st1204;
	}
	goto tr241;
st1204:
	if ( ++( p) == ( pe) )
		goto _test_eof1204;
case 1204:
	switch( (*( p)) ) {
		case 84: goto st1205;
		case 116: goto st1205;
	}
	goto tr241;
st1205:
	if ( ++( p) == ( pe) )
		goto _test_eof1205;
case 1205:
	switch( (*( p)) ) {
		case 80: goto st1206;
		case 112: goto st1206;
	}
	goto tr241;
st1206:
	if ( ++( p) == ( pe) )
		goto _test_eof1206;
case 1206:
	switch( (*( p)) ) {
		case 58: goto st1207;
		case 83: goto st1210;
		case 115: goto st1210;
	}
	goto tr241;
st1207:
	if ( ++( p) == ( pe) )
		goto _test_eof1207;
case 1207:
	if ( (*( p)) == 47 )
		goto st1208;
	goto tr241;
st1208:
	if ( ++( p) == ( pe) )
		goto _test_eof1208;
case 1208:
	if ( (*( p)) == 47 )
		goto st1209;
	goto tr241;
st1209:
	if ( ++( p) == ( pe) )
		goto _test_eof1209;
case 1209:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1191;
st1210:
	if ( ++( p) == ( pe) )
		goto _test_eof1210;
case 1210:
	if ( (*( p)) == 58 )
		goto st1207;
	goto tr241;
tr2377:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1211;
tr1429:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1211;
st1211:
	if ( ++( p) == ( pe) )
		goto _test_eof1211;
case 1211:
#line 25778 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr582;
		case 9: goto tr1429;
		case 10: goto tr584;
		case 13: goto tr584;
		case 32: goto tr1429;
		case 58: goto tr1431;
		case 60: goto tr1432;
		case 62: goto tr1433;
		case 92: goto tr1434;
		case 93: goto tr950;
		case 124: goto tr1435;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr1430;
	goto tr1428;
tr1428:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1212;
st1212:
	if ( ++( p) == ( pe) )
		goto _test_eof1212;
case 1212:
#line 25803 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st503;
		case 9: goto tr1437;
		case 10: goto st505;
		case 13: goto st505;
		case 32: goto tr1437;
		case 35: goto tr1439;
		case 93: goto tr1440;
		case 124: goto tr1441;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1214;
	goto st1212;
tr1437:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1213;
st1213:
	if ( ++( p) == ( pe) )
		goto _test_eof1213;
case 1213:
#line 25825 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st503;
		case 9: goto st1213;
		case 10: goto st505;
		case 13: goto st505;
		case 32: goto st1213;
		case 35: goto st1215;
		case 93: goto tr1444;
		case 124: goto st1219;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1214;
	goto st1212;
tr1430:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1214;
st1214:
	if ( ++( p) == ( pe) )
		goto _test_eof1214;
case 1214:
#line 25847 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st503;
		case 10: goto st505;
		case 13: goto st505;
		case 32: goto st1214;
		case 93: goto tr950;
		case 124: goto st783;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st1214;
	goto st1212;
tr1439:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1215;
st1215:
	if ( ++( p) == ( pe) )
		goto _test_eof1215;
case 1215:
#line 25867 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st503;
		case 9: goto tr1437;
		case 10: goto st505;
		case 13: goto st505;
		case 32: goto tr1437;
		case 35: goto tr1439;
		case 93: goto tr1440;
		case 124: goto tr1441;
	}
	if ( (*( p)) > 12 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr1446;
	} else if ( (*( p)) >= 11 )
		goto st1214;
	goto st1212;
tr1446:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1216;
st1216:
	if ( ++( p) == ( pe) )
		goto _test_eof1216;
case 1216:
#line 25892 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1447;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1448;
		case 45: goto st1224;
		case 93: goto tr1451;
		case 95: goto st1224;
		case 124: goto tr1452;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1216;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1216;
	} else
		goto st1216;
	goto st783;
tr1447:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1217;
st1217:
	if ( ++( p) == ( pe) )
		goto _test_eof1217;
case 1217:
#line 25921 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1217;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1217;
		case 93: goto tr1444;
		case 124: goto st1219;
	}
	goto st783;
tr1444:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1218;
tr1440:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1218;
tr1451:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1218;
st1218:
	if ( ++( p) == ( pe) )
		goto _test_eof1218;
case 1218:
#line 25952 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st785;
		case 93: goto st1682;
	}
	goto tr241;
tr1441:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1219;
tr1452:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1219;
tr1455:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1219;
st1219:
	if ( ++( p) == ( pe) )
		goto _test_eof1219;
case 1219:
#line 25976 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr609;
		case 9: goto tr1455;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1455;
		case 93: goto tr1456;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto tr1454;
tr1454:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1220;
st1220:
	if ( ++( p) == ( pe) )
		goto _test_eof1220;
case 1220:
#line 25997 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto tr1458;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1458;
		case 93: goto tr1459;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1220;
tr1458:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1221;
st1221:
	if ( ++( p) == ( pe) )
		goto _test_eof1221;
case 1221:
#line 26018 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto st1221;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1221;
		case 93: goto tr1461;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1220;
tr1461:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1222;
tr1456:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1222;
tr1459:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1222;
st1222:
	if ( ++( p) == ( pe) )
		goto _test_eof1222;
case 1222:
#line 26053 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st785;
		case 93: goto st1684;
	}
	goto tr241;
tr1448:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1223;
st1223:
	if ( ++( p) == ( pe) )
		goto _test_eof1223;
case 1223:
#line 26067 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1217;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1223;
		case 45: goto st1224;
		case 93: goto tr1444;
		case 95: goto st1224;
		case 124: goto st1219;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1216;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1216;
	} else
		goto st1216;
	goto st783;
st1224:
	if ( ++( p) == ( pe) )
		goto _test_eof1224;
case 1224:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1224;
		case 45: goto st1224;
		case 93: goto tr950;
		case 95: goto st1224;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1216;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1216;
	} else
		goto st1216;
	goto st783;
tr1431:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1225;
st1225:
	if ( ++( p) == ( pe) )
		goto _test_eof1225;
case 1225:
#line 26118 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st503;
		case 9: goto tr1437;
		case 10: goto st505;
		case 13: goto st505;
		case 32: goto tr1437;
		case 35: goto tr1439;
		case 93: goto tr1440;
		case 124: goto tr1463;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1214;
	goto st1212;
tr1463:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1226;
st1226:
	if ( ++( p) == ( pe) )
		goto _test_eof1226;
case 1226:
#line 26140 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr609;
		case 9: goto tr1464;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1464;
		case 35: goto tr1465;
		case 93: goto tr1466;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto tr1454;
tr1467:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1227;
tr1464:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1227;
st1227:
	if ( ++( p) == ( pe) )
		goto _test_eof1227;
case 1227:
#line 26172 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr609;
		case 9: goto tr1467;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1467;
		case 35: goto tr1468;
		case 93: goto tr1469;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto tr1454;
tr1502:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1228;
tr1468:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1228;
tr1465:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1228;
st1228:
	if ( ++( p) == ( pe) )
		goto _test_eof1228;
case 1228:
#line 26204 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto tr1458;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1458;
		case 93: goto tr1459;
		case 124: goto st783;
	}
	if ( (*( p)) > 12 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr1470;
	} else if ( (*( p)) >= 11 )
		goto st783;
	goto st1220;
tr1470:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1229;
st1229:
	if ( ++( p) == ( pe) )
		goto _test_eof1229;
case 1229:
#line 26228 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto tr1471;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1472;
		case 45: goto st1233;
		case 93: goto tr1475;
		case 95: goto st1233;
		case 124: goto st783;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st783;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1229;
		} else if ( (*( p)) >= 65 )
			goto st1229;
	} else
		goto st1229;
	goto st1220;
tr1471:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1230;
st1230:
	if ( ++( p) == ( pe) )
		goto _test_eof1230;
case 1230:
#line 26262 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto st1230;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1230;
		case 93: goto tr1477;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1220;
tr1477:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1231;
tr1469:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1231;
tr1466:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1231;
tr1475:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1231;
tr1503:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1231;
st1231:
	if ( ++( p) == ( pe) )
		goto _test_eof1231;
case 1231:
#line 26317 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st785;
		case 93: goto st1686;
	}
	goto tr241;
tr1472:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1232;
st1232:
	if ( ++( p) == ( pe) )
		goto _test_eof1232;
case 1232:
#line 26333 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto st1230;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1232;
		case 45: goto st1233;
		case 93: goto tr1477;
		case 95: goto st1233;
		case 124: goto st783;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st783;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1229;
		} else if ( (*( p)) >= 65 )
			goto st1229;
	} else
		goto st1229;
	goto st1220;
st1233:
	if ( ++( p) == ( pe) )
		goto _test_eof1233;
case 1233:
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto tr1458;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1479;
		case 45: goto st1233;
		case 93: goto tr1459;
		case 95: goto st1233;
		case 124: goto st783;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st783;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1229;
		} else if ( (*( p)) >= 65 )
			goto st1229;
	} else
		goto st1229;
	goto st1220;
tr1479:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1234;
st1234:
	if ( ++( p) == ( pe) )
		goto _test_eof1234;
case 1234:
#line 26392 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto st1221;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1234;
		case 45: goto st1233;
		case 93: goto tr1461;
		case 95: goto st1233;
		case 124: goto st783;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st783;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1229;
		} else if ( (*( p)) >= 65 )
			goto st1229;
	} else
		goto st1229;
	goto st1220;
tr1432:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1235;
st1235:
	if ( ++( p) == ( pe) )
		goto _test_eof1235;
case 1235:
#line 26424 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st503;
		case 9: goto tr1437;
		case 10: goto st505;
		case 13: goto st505;
		case 32: goto tr1437;
		case 35: goto tr1439;
		case 93: goto tr1440;
		case 124: goto tr1481;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1214;
	goto st1212;
tr1481:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1236;
st1236:
	if ( ++( p) == ( pe) )
		goto _test_eof1236;
case 1236:
#line 26446 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr609;
		case 9: goto tr1455;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1455;
		case 62: goto tr1482;
		case 93: goto tr1456;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto tr1454;
tr1482:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1237;
st1237:
	if ( ++( p) == ( pe) )
		goto _test_eof1237;
case 1237:
#line 26468 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto tr1458;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1458;
		case 93: goto tr1459;
		case 95: goto st1238;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1220;
st1238:
	if ( ++( p) == ( pe) )
		goto _test_eof1238;
case 1238:
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto tr1458;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1458;
		case 60: goto st1239;
		case 93: goto tr1459;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1220;
st1239:
	if ( ++( p) == ( pe) )
		goto _test_eof1239;
case 1239:
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto tr1458;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1458;
		case 93: goto tr1459;
		case 124: goto st1240;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1220;
st1240:
	if ( ++( p) == ( pe) )
		goto _test_eof1240;
case 1240:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 62: goto st1241;
		case 93: goto tr950;
	}
	goto st783;
st1241:
	if ( ++( p) == ( pe) )
		goto _test_eof1241;
case 1241:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1487;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1487;
		case 35: goto tr1488;
		case 93: goto tr1440;
	}
	goto st783;
tr1487:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1242;
st1242:
	if ( ++( p) == ( pe) )
		goto _test_eof1242;
case 1242:
#line 26549 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1242;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1242;
		case 35: goto st1243;
		case 93: goto tr1444;
	}
	goto st783;
tr1488:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1243;
st1243:
	if ( ++( p) == ( pe) )
		goto _test_eof1243;
case 1243:
#line 26568 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
	}
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr1491;
	goto st783;
tr1491:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1244;
st1244:
	if ( ++( p) == ( pe) )
		goto _test_eof1244;
case 1244:
#line 26586 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1492;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1493;
		case 45: goto st1247;
		case 93: goto tr1451;
		case 95: goto st1247;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1244;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1244;
	} else
		goto st1244;
	goto st783;
tr1492:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1245;
st1245:
	if ( ++( p) == ( pe) )
		goto _test_eof1245;
case 1245:
#line 26614 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1245;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1245;
		case 93: goto tr1444;
	}
	goto st783;
tr1493:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1246;
st1246:
	if ( ++( p) == ( pe) )
		goto _test_eof1246;
case 1246:
#line 26632 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1245;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1246;
		case 45: goto st1247;
		case 93: goto tr1444;
		case 95: goto st1247;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1244;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1244;
	} else
		goto st1244;
	goto st783;
st1247:
	if ( ++( p) == ( pe) )
		goto _test_eof1247;
case 1247:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1247;
		case 45: goto st1247;
		case 93: goto tr950;
		case 95: goto st1247;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1244;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1244;
	} else
		goto st1244;
	goto st783;
tr1433:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1248;
st1248:
	if ( ++( p) == ( pe) )
		goto _test_eof1248;
case 1248:
#line 26682 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st503;
		case 9: goto tr1437;
		case 10: goto st505;
		case 13: goto st505;
		case 32: goto tr1437;
		case 35: goto tr1439;
		case 58: goto st1225;
		case 93: goto tr1440;
		case 124: goto tr1499;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1214;
	goto st1212;
tr1499:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1249;
st1249:
	if ( ++( p) == ( pe) )
		goto _test_eof1249;
case 1249:
#line 26705 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr609;
		case 9: goto tr1455;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1455;
		case 51: goto tr1500;
		case 93: goto tr1456;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto tr1454;
tr1500:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1250;
st1250:
	if ( ++( p) == ( pe) )
		goto _test_eof1250;
case 1250:
#line 26727 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto tr1501;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1501;
		case 35: goto tr1502;
		case 93: goto tr1503;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1220;
tr1501:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1251;
st1251:
	if ( ++( p) == ( pe) )
		goto _test_eof1251;
case 1251:
#line 26751 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st511;
		case 9: goto st1251;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1251;
		case 35: goto st1228;
		case 93: goto tr1477;
		case 124: goto st783;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto st1220;
tr1434:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1252;
st1252:
	if ( ++( p) == ( pe) )
		goto _test_eof1252;
case 1252:
#line 26773 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st503;
		case 9: goto tr1437;
		case 10: goto st505;
		case 13: goto st505;
		case 32: goto tr1437;
		case 35: goto tr1439;
		case 93: goto tr1440;
		case 124: goto tr1506;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1214;
	goto st1212;
tr1506:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1253;
st1253:
	if ( ++( p) == ( pe) )
		goto _test_eof1253;
case 1253:
#line 26795 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr609;
		case 9: goto tr1455;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1455;
		case 93: goto tr1456;
		case 124: goto st1254;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st783;
	goto tr1454;
st1254:
	if ( ++( p) == ( pe) )
		goto _test_eof1254;
case 1254:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1241;
		case 93: goto tr950;
	}
	goto st783;
tr1435:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1255;
st1255:
	if ( ++( p) == ( pe) )
		goto _test_eof1255;
case 1255:
#line 26828 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
		case 95: goto st1259;
		case 119: goto st1260;
		case 124: goto st1261;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1256;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1256;
	} else
		goto st1256;
	goto st783;
st1256:
	if ( ++( p) == ( pe) )
		goto _test_eof1256;
case 1256:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1512;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1512;
		case 35: goto tr1513;
		case 93: goto tr1440;
		case 124: goto tr1441;
	}
	goto st783;
tr1512:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1257;
st1257:
	if ( ++( p) == ( pe) )
		goto _test_eof1257;
case 1257:
#line 26870 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto st1257;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto st1257;
		case 35: goto st1258;
		case 93: goto tr1444;
		case 124: goto st1219;
	}
	goto st783;
tr1513:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1258;
st1258:
	if ( ++( p) == ( pe) )
		goto _test_eof1258;
case 1258:
#line 26890 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
	}
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr1446;
	goto st783;
st1259:
	if ( ++( p) == ( pe) )
		goto _test_eof1259;
case 1259:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
		case 124: goto st1256;
	}
	goto st783;
st1260:
	if ( ++( p) == ( pe) )
		goto _test_eof1260;
case 1260:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 9: goto tr1512;
		case 10: goto tr241;
		case 13: goto tr241;
		case 32: goto tr1512;
		case 35: goto tr1513;
		case 93: goto tr1440;
		case 124: goto tr1463;
	}
	goto st783;
st1261:
	if ( ++( p) == ( pe) )
		goto _test_eof1261;
case 1261:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
		case 95: goto st1262;
	}
	goto st783;
st1262:
	if ( ++( p) == ( pe) )
		goto _test_eof1262;
case 1262:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 93: goto tr950;
		case 124: goto st1259;
	}
	goto st783;
tr2098:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1878;
st1878:
	if ( ++( p) == ( pe) )
		goto _test_eof1878;
case 1878:
#line 26965 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 123 )
		goto st555;
	goto tr2102;
tr2099:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1879;
st1879:
	if ( ++( p) == ( pe) )
		goto _test_eof1879;
case 1879:
#line 26981 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1263;
		case 65: goto st1274;
		case 66: goto st1297;
		case 67: goto st1299;
		case 69: goto st1306;
		case 72: goto tr2388;
		case 73: goto st1307;
		case 78: goto st1317;
		case 81: goto st239;
		case 83: goto st1324;
		case 84: goto st1337;
		case 85: goto st1339;
		case 97: goto st1274;
		case 98: goto st1297;
		case 99: goto st1299;
		case 101: goto st1306;
		case 104: goto tr2388;
		case 105: goto st1307;
		case 110: goto st1317;
		case 113: goto st239;
		case 115: goto st1324;
		case 116: goto st1337;
		case 117: goto st1339;
	}
	goto tr2102;
st1263:
	if ( ++( p) == ( pe) )
		goto _test_eof1263;
case 1263:
	switch( (*( p)) ) {
		case 66: goto st1264;
		case 69: goto st1265;
		case 73: goto st1266;
		case 81: goto st226;
		case 83: goto st1267;
		case 84: goto st302;
		case 85: goto st1273;
		case 98: goto st1264;
		case 101: goto st1265;
		case 105: goto st1266;
		case 113: goto st226;
		case 115: goto st1267;
		case 116: goto st302;
		case 117: goto st1273;
	}
	goto tr241;
st1264:
	if ( ++( p) == ( pe) )
		goto _test_eof1264;
case 1264:
	switch( (*( p)) ) {
		case 62: goto tr992;
		case 76: goto st211;
		case 108: goto st211;
	}
	goto tr241;
st1265:
	if ( ++( p) == ( pe) )
		goto _test_eof1265;
case 1265:
	switch( (*( p)) ) {
		case 77: goto st1266;
		case 88: goto st221;
		case 109: goto st1266;
		case 120: goto st221;
	}
	goto tr241;
st1266:
	if ( ++( p) == ( pe) )
		goto _test_eof1266;
case 1266:
	if ( (*( p)) == 62 )
		goto tr1008;
	goto tr241;
st1267:
	if ( ++( p) == ( pe) )
		goto _test_eof1267;
case 1267:
	switch( (*( p)) ) {
		case 62: goto tr1019;
		case 80: goto st295;
		case 84: goto st1268;
		case 112: goto st295;
		case 116: goto st1268;
	}
	goto tr241;
st1268:
	if ( ++( p) == ( pe) )
		goto _test_eof1268;
case 1268:
	switch( (*( p)) ) {
		case 82: goto st1269;
		case 114: goto st1269;
	}
	goto tr241;
st1269:
	if ( ++( p) == ( pe) )
		goto _test_eof1269;
case 1269:
	switch( (*( p)) ) {
		case 79: goto st1270;
		case 111: goto st1270;
	}
	goto tr241;
st1270:
	if ( ++( p) == ( pe) )
		goto _test_eof1270;
case 1270:
	switch( (*( p)) ) {
		case 78: goto st1271;
		case 110: goto st1271;
	}
	goto tr241;
st1271:
	if ( ++( p) == ( pe) )
		goto _test_eof1271;
case 1271:
	switch( (*( p)) ) {
		case 71: goto st1272;
		case 103: goto st1272;
	}
	goto tr241;
st1272:
	if ( ++( p) == ( pe) )
		goto _test_eof1272;
case 1272:
	if ( (*( p)) == 62 )
		goto tr992;
	goto tr241;
st1273:
	if ( ++( p) == ( pe) )
		goto _test_eof1273;
case 1273:
	if ( (*( p)) == 62 )
		goto tr1037;
	goto tr241;
st1274:
	if ( ++( p) == ( pe) )
		goto _test_eof1274;
case 1274:
	switch( (*( p)) ) {
		case 9: goto st1275;
		case 32: goto st1275;
	}
	goto tr241;
st1275:
	if ( ++( p) == ( pe) )
		goto _test_eof1275;
case 1275:
	switch( (*( p)) ) {
		case 9: goto st1275;
		case 32: goto st1275;
		case 72: goto st1276;
		case 104: goto st1276;
	}
	goto tr241;
st1276:
	if ( ++( p) == ( pe) )
		goto _test_eof1276;
case 1276:
	switch( (*( p)) ) {
		case 82: goto st1277;
		case 114: goto st1277;
	}
	goto tr241;
st1277:
	if ( ++( p) == ( pe) )
		goto _test_eof1277;
case 1277:
	switch( (*( p)) ) {
		case 69: goto st1278;
		case 101: goto st1278;
	}
	goto tr241;
st1278:
	if ( ++( p) == ( pe) )
		goto _test_eof1278;
case 1278:
	switch( (*( p)) ) {
		case 70: goto st1279;
		case 102: goto st1279;
	}
	goto tr241;
st1279:
	if ( ++( p) == ( pe) )
		goto _test_eof1279;
case 1279:
	if ( (*( p)) == 61 )
		goto st1280;
	goto tr241;
st1280:
	if ( ++( p) == ( pe) )
		goto _test_eof1280;
case 1280:
	if ( (*( p)) == 34 )
		goto st1281;
	goto tr241;
st1281:
	if ( ++( p) == ( pe) )
		goto _test_eof1281;
case 1281:
	switch( (*( p)) ) {
		case 35: goto tr1534;
		case 47: goto tr1534;
		case 72: goto tr1535;
		case 104: goto tr1535;
	}
	goto tr241;
tr1534:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1282;
st1282:
	if ( ++( p) == ( pe) )
		goto _test_eof1282;
case 1282:
#line 27199 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1537;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1282;
tr1537:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1283;
st1283:
	if ( ++( p) == ( pe) )
		goto _test_eof1283;
case 1283:
#line 27216 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 34: goto tr1537;
		case 62: goto st1284;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1282;
st1284:
	if ( ++( p) == ( pe) )
		goto _test_eof1284;
case 1284:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
	}
	goto tr1539;
tr1539:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1285;
st1285:
	if ( ++( p) == ( pe) )
		goto _test_eof1285;
case 1285:
#line 27244 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 60: goto tr1541;
	}
	goto st1285;
tr1541:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1286;
st1286:
	if ( ++( p) == ( pe) )
		goto _test_eof1286;
case 1286:
#line 27260 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 47: goto st1287;
		case 60: goto tr1541;
	}
	goto st1285;
st1287:
	if ( ++( p) == ( pe) )
		goto _test_eof1287;
case 1287:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 60: goto tr1541;
		case 65: goto st1288;
		case 97: goto st1288;
	}
	goto st1285;
st1288:
	if ( ++( p) == ( pe) )
		goto _test_eof1288;
case 1288:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 10: goto tr241;
		case 13: goto tr241;
		case 60: goto tr1541;
		case 62: goto tr1544;
	}
	goto st1285;
tr1535:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1289;
st1289:
	if ( ++( p) == ( pe) )
		goto _test_eof1289;
case 1289:
#line 27302 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1290;
		case 116: goto st1290;
	}
	goto tr241;
st1290:
	if ( ++( p) == ( pe) )
		goto _test_eof1290;
case 1290:
	switch( (*( p)) ) {
		case 84: goto st1291;
		case 116: goto st1291;
	}
	goto tr241;
st1291:
	if ( ++( p) == ( pe) )
		goto _test_eof1291;
case 1291:
	switch( (*( p)) ) {
		case 80: goto st1292;
		case 112: goto st1292;
	}
	goto tr241;
st1292:
	if ( ++( p) == ( pe) )
		goto _test_eof1292;
case 1292:
	switch( (*( p)) ) {
		case 58: goto st1293;
		case 83: goto st1296;
		case 115: goto st1296;
	}
	goto tr241;
st1293:
	if ( ++( p) == ( pe) )
		goto _test_eof1293;
case 1293:
	if ( (*( p)) == 47 )
		goto st1294;
	goto tr241;
st1294:
	if ( ++( p) == ( pe) )
		goto _test_eof1294;
case 1294:
	if ( (*( p)) == 47 )
		goto st1295;
	goto tr241;
st1295:
	if ( ++( p) == ( pe) )
		goto _test_eof1295;
case 1295:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1282;
st1296:
	if ( ++( p) == ( pe) )
		goto _test_eof1296;
case 1296:
	if ( (*( p)) == 58 )
		goto st1293;
	goto tr241;
st1297:
	if ( ++( p) == ( pe) )
		goto _test_eof1297;
case 1297:
	switch( (*( p)) ) {
		case 62: goto tr1039;
		case 76: goto st235;
		case 82: goto st1298;
		case 108: goto st235;
		case 114: goto st1298;
	}
	goto tr241;
st1298:
	if ( ++( p) == ( pe) )
		goto _test_eof1298;
case 1298:
	if ( (*( p)) == 62 )
		goto tr1040;
	goto tr241;
st1299:
	if ( ++( p) == ( pe) )
		goto _test_eof1299;
case 1299:
	switch( (*( p)) ) {
		case 79: goto st1300;
		case 111: goto st1300;
	}
	goto tr241;
st1300:
	if ( ++( p) == ( pe) )
		goto _test_eof1300;
case 1300:
	switch( (*( p)) ) {
		case 68: goto st1301;
		case 100: goto st1301;
	}
	goto tr241;
st1301:
	if ( ++( p) == ( pe) )
		goto _test_eof1301;
case 1301:
	switch( (*( p)) ) {
		case 69: goto st1302;
		case 101: goto st1302;
	}
	goto tr241;
st1302:
	if ( ++( p) == ( pe) )
		goto _test_eof1302;
case 1302:
	switch( (*( p)) ) {
		case 9: goto st1303;
		case 32: goto st1303;
		case 61: goto st1304;
		case 62: goto tr1047;
	}
	goto tr241;
st1303:
	if ( ++( p) == ( pe) )
		goto _test_eof1303;
case 1303:
	switch( (*( p)) ) {
		case 9: goto st1303;
		case 32: goto st1303;
		case 61: goto st1304;
	}
	goto tr241;
st1304:
	if ( ++( p) == ( pe) )
		goto _test_eof1304;
case 1304:
	switch( (*( p)) ) {
		case 9: goto st1304;
		case 32: goto st1304;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1558;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1558;
	} else
		goto tr1558;
	goto tr241;
tr1558:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1305;
st1305:
	if ( ++( p) == ( pe) )
		goto _test_eof1305;
case 1305:
#line 27460 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 62 )
		goto tr1560;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1305;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1305;
	} else
		goto st1305;
	goto tr241;
tr1560:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1880;
st1880:
	if ( ++( p) == ( pe) )
		goto _test_eof1880;
case 1880:
#line 27482 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1052;
		case 9: goto st870;
		case 10: goto tr1052;
		case 32: goto st870;
	}
	goto tr2379;
st1306:
	if ( ++( p) == ( pe) )
		goto _test_eof1306;
case 1306:
	switch( (*( p)) ) {
		case 77: goto st1307;
		case 109: goto st1307;
	}
	goto tr241;
st1307:
	if ( ++( p) == ( pe) )
		goto _test_eof1307;
case 1307:
	if ( (*( p)) == 62 )
		goto tr1249;
	goto tr241;
tr2388:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1308;
st1308:
	if ( ++( p) == ( pe) )
		goto _test_eof1308;
case 1308:
#line 27514 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1309;
		case 116: goto st1309;
	}
	goto tr241;
st1309:
	if ( ++( p) == ( pe) )
		goto _test_eof1309;
case 1309:
	switch( (*( p)) ) {
		case 84: goto st1310;
		case 116: goto st1310;
	}
	goto tr241;
st1310:
	if ( ++( p) == ( pe) )
		goto _test_eof1310;
case 1310:
	switch( (*( p)) ) {
		case 80: goto st1311;
		case 112: goto st1311;
	}
	goto tr241;
st1311:
	if ( ++( p) == ( pe) )
		goto _test_eof1311;
case 1311:
	switch( (*( p)) ) {
		case 58: goto st1312;
		case 83: goto st1316;
		case 115: goto st1316;
	}
	goto tr241;
st1312:
	if ( ++( p) == ( pe) )
		goto _test_eof1312;
case 1312:
	if ( (*( p)) == 47 )
		goto st1313;
	goto tr241;
st1313:
	if ( ++( p) == ( pe) )
		goto _test_eof1313;
case 1313:
	if ( (*( p)) == 47 )
		goto st1314;
	goto tr241;
st1314:
	if ( ++( p) == ( pe) )
		goto _test_eof1314;
case 1314:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1315;
st1315:
	if ( ++( p) == ( pe) )
		goto _test_eof1315;
case 1315:
	switch( (*( p)) ) {
		case 0: goto tr241;
		case 32: goto tr241;
		case 62: goto tr1570;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr241;
	goto st1315;
st1316:
	if ( ++( p) == ( pe) )
		goto _test_eof1316;
case 1316:
	if ( (*( p)) == 58 )
		goto st1312;
	goto tr241;
st1317:
	if ( ++( p) == ( pe) )
		goto _test_eof1317;
case 1317:
	switch( (*( p)) ) {
		case 79: goto st1318;
		case 111: goto st1318;
	}
	goto tr241;
st1318:
	if ( ++( p) == ( pe) )
		goto _test_eof1318;
case 1318:
	switch( (*( p)) ) {
		case 68: goto st1319;
		case 100: goto st1319;
	}
	goto tr241;
st1319:
	if ( ++( p) == ( pe) )
		goto _test_eof1319;
case 1319:
	switch( (*( p)) ) {
		case 84: goto st1320;
		case 116: goto st1320;
	}
	goto tr241;
st1320:
	if ( ++( p) == ( pe) )
		goto _test_eof1320;
case 1320:
	switch( (*( p)) ) {
		case 69: goto st1321;
		case 101: goto st1321;
	}
	goto tr241;
st1321:
	if ( ++( p) == ( pe) )
		goto _test_eof1321;
case 1321:
	switch( (*( p)) ) {
		case 88: goto st1322;
		case 120: goto st1322;
	}
	goto tr241;
st1322:
	if ( ++( p) == ( pe) )
		goto _test_eof1322;
case 1322:
	switch( (*( p)) ) {
		case 84: goto st1323;
		case 116: goto st1323;
	}
	goto tr241;
st1323:
	if ( ++( p) == ( pe) )
		goto _test_eof1323;
case 1323:
	if ( (*( p)) == 62 )
		goto tr1256;
	goto tr241;
st1324:
	if ( ++( p) == ( pe) )
		goto _test_eof1324;
case 1324:
	switch( (*( p)) ) {
		case 62: goto tr1265;
		case 80: goto st1325;
		case 84: goto st1332;
		case 112: goto st1325;
		case 116: goto st1332;
	}
	goto tr241;
st1325:
	if ( ++( p) == ( pe) )
		goto _test_eof1325;
case 1325:
	switch( (*( p)) ) {
		case 79: goto st1326;
		case 111: goto st1326;
	}
	goto tr241;
st1326:
	if ( ++( p) == ( pe) )
		goto _test_eof1326;
case 1326:
	switch( (*( p)) ) {
		case 73: goto st1327;
		case 105: goto st1327;
	}
	goto tr241;
st1327:
	if ( ++( p) == ( pe) )
		goto _test_eof1327;
case 1327:
	switch( (*( p)) ) {
		case 76: goto st1328;
		case 108: goto st1328;
	}
	goto tr241;
st1328:
	if ( ++( p) == ( pe) )
		goto _test_eof1328;
case 1328:
	switch( (*( p)) ) {
		case 69: goto st1329;
		case 101: goto st1329;
	}
	goto tr241;
st1329:
	if ( ++( p) == ( pe) )
		goto _test_eof1329;
case 1329:
	switch( (*( p)) ) {
		case 82: goto st1330;
		case 114: goto st1330;
	}
	goto tr241;
st1330:
	if ( ++( p) == ( pe) )
		goto _test_eof1330;
case 1330:
	switch( (*( p)) ) {
		case 62: goto tr1272;
		case 83: goto st1331;
		case 115: goto st1331;
	}
	goto tr241;
st1331:
	if ( ++( p) == ( pe) )
		goto _test_eof1331;
case 1331:
	if ( (*( p)) == 62 )
		goto tr1272;
	goto tr241;
st1332:
	if ( ++( p) == ( pe) )
		goto _test_eof1332;
case 1332:
	switch( (*( p)) ) {
		case 82: goto st1333;
		case 114: goto st1333;
	}
	goto tr241;
st1333:
	if ( ++( p) == ( pe) )
		goto _test_eof1333;
case 1333:
	switch( (*( p)) ) {
		case 79: goto st1334;
		case 111: goto st1334;
	}
	goto tr241;
st1334:
	if ( ++( p) == ( pe) )
		goto _test_eof1334;
case 1334:
	switch( (*( p)) ) {
		case 78: goto st1335;
		case 110: goto st1335;
	}
	goto tr241;
st1335:
	if ( ++( p) == ( pe) )
		goto _test_eof1335;
case 1335:
	switch( (*( p)) ) {
		case 71: goto st1336;
		case 103: goto st1336;
	}
	goto tr241;
st1336:
	if ( ++( p) == ( pe) )
		goto _test_eof1336;
case 1336:
	if ( (*( p)) == 62 )
		goto tr1039;
	goto tr241;
st1337:
	if ( ++( p) == ( pe) )
		goto _test_eof1337;
case 1337:
	switch( (*( p)) ) {
		case 78: goto st1338;
		case 110: goto st1338;
	}
	goto tr241;
st1338:
	if ( ++( p) == ( pe) )
		goto _test_eof1338;
case 1338:
	if ( (*( p)) == 62 )
		goto tr1282;
	goto tr241;
st1339:
	if ( ++( p) == ( pe) )
		goto _test_eof1339;
case 1339:
	if ( (*( p)) == 62 )
		goto tr1284;
	goto tr241;
tr2100:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1881;
st1881:
	if ( ++( p) == ( pe) )
		goto _test_eof1881;
case 1881:
#line 27805 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( 64 <= (*( p)) && (*( p)) <= 64 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 47: goto st1263;
		case 65: goto st1274;
		case 66: goto st1297;
		case 67: goto st1299;
		case 69: goto st1306;
		case 72: goto tr2388;
		case 73: goto st1307;
		case 78: goto st1317;
		case 81: goto st239;
		case 83: goto st1324;
		case 84: goto st1337;
		case 85: goto st1339;
		case 97: goto st1274;
		case 98: goto st1297;
		case 99: goto st1299;
		case 101: goto st1306;
		case 104: goto tr2388;
		case 105: goto st1307;
		case 110: goto st1317;
		case 113: goto st239;
		case 115: goto st1324;
		case 116: goto st1337;
		case 117: goto st1339;
		case 1088: goto st1340;
	}
	goto tr2102;
st1340:
	if ( ++( p) == ( pe) )
		goto _test_eof1340;
case 1340:
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) <= -1 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) > 31 ) {
			if ( 33 <= (*( p)) )
 {				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) >= 14 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( _widec < 1025 ) {
		if ( 896 <= _widec && _widec <= 1023 )
			goto tr1590;
	} else if ( _widec > 1032 ) {
		if ( _widec > 1055 ) {
			if ( 1057 <= _widec && _widec <= 1151 )
				goto tr1590;
		} else if ( _widec >= 1038 )
			goto tr1590;
	} else
		goto tr1590;
	goto tr241;
tr1591:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1341;
tr1590:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1341;
st1341:
	if ( ++( p) == ( pe) )
		goto _test_eof1341;
case 1341:
#line 27898 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) < 11 ) {
		if ( (*( p)) > -1 ) {
			if ( 1 <= (*( p)) && (*( p)) <= 9 ) {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 12 ) {
		if ( (*( p)) < 62 ) {
			if ( 14 <= (*( p)) && (*( p)) <= 61 ) {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 62 ) {
			if ( 63 <= (*( p)) )
 {				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( _widec == 1086 )
		goto tr1592;
	if ( _widec < 1025 ) {
		if ( 896 <= _widec && _widec <= 1023 )
			goto tr1591;
	} else if ( _widec > 1033 ) {
		if ( _widec > 1036 ) {
			if ( 1038 <= _widec && _widec <= 1151 )
				goto tr1591;
		} else if ( _widec >= 1035 )
			goto tr1591;
	} else
		goto tr1591;
	goto tr241;
tr2101:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 596 "ext/dtext/dtext.cpp.rl"
	{( act) = 99;}
	goto st1882;
st1882:
	if ( ++( p) == ( pe) )
		goto _test_eof1882;
case 1882:
#line 27965 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -29 ) {
			if ( (*( p)) < -32 ) {
				if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -31 ) {
				if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -29 ) {
			if ( (*( p)) < -17 ) {
				if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( -16 <= (*( p)) && (*( p)) <= -12 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 65 ) {
			if ( (*( p)) < 46 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 46 ) {
				if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 90 ) {
			if ( (*( p)) < 97 ) {
				if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 122 ) {
				if ( 127 <= (*( p)) )
 {					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto tr2396;
		case 995: goto tr2397;
		case 1007: goto tr2398;
		case 1070: goto tr2401;
		case 1119: goto tr2401;
		case 1151: goto tr2400;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( 962 <= _widec && _widec <= 991 )
				goto tr2394;
		} else if ( _widec > 1006 ) {
			if ( 1008 <= _widec && _widec <= 1012 )
				goto tr2399;
		} else
			goto tr2395;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( 1038 <= _widec && _widec <= 1055 )
				goto tr2400;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr2400;
			} else if ( _widec >= 1089 )
				goto tr2400;
		} else
			goto tr2400;
	} else
		goto tr2400;
	goto tr2102;
tr2394:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1342;
st1342:
	if ( ++( p) == ( pe) )
		goto _test_eof1342;
case 1342:
#line 28111 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1343;
	goto tr241;
tr2400:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1343;
st1343:
	if ( ++( p) == ( pe) )
		goto _test_eof1343;
case 1343:
#line 28130 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -29 ) {
			if ( (*( p)) < -62 ) {
				if ( (*( p)) <= -63 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -33 ) {
				if ( (*( p)) > -31 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -32 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -29 ) {
			if ( (*( p)) < -17 ) {
				if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1357;
		case 995: goto st1359;
		case 1007: goto st1361;
		case 1057: goto st1343;
		case 1063: goto st1365;
		case 1067: goto st1343;
		case 1119: goto st1343;
		case 1151: goto tr1601;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1355;
			} else if ( _widec >= 896 )
				goto st1344;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1344;
			} else if ( _widec >= 1008 )
				goto st1364;
		} else
			goto st1356;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1343;
			} else if ( _widec >= 1038 )
				goto tr1601;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1601;
			} else if ( _widec >= 1089 )
				goto tr1601;
		} else
			goto tr1601;
	} else
		goto tr1601;
	goto tr234;
st1344:
	if ( ++( p) == ( pe) )
		goto _test_eof1344;
case 1344:
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -29 ) {
			if ( (*( p)) < -62 ) {
				if ( (*( p)) <= -63 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -33 ) {
				if ( (*( p)) > -31 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -32 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -29 ) {
			if ( (*( p)) < -17 ) {
				if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1345;
			} else if ( _widec >= 896 )
				goto st1344;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1344;
			} else if ( _widec >= 1008 )
				goto st1353;
		} else
			goto st1346;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1344;
			} else if ( _widec >= 1038 )
				goto tr1609;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else if ( _widec >= 1089 )
				goto tr1609;
		} else
			goto tr1609;
	} else
		goto tr1609;
	goto tr234;
st1345:
	if ( ++( p) == ( pe) )
		goto _test_eof1345;
case 1345:
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -30 ) {
			if ( (*( p)) < -64 ) {
				if ( (*( p)) <= -65 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -63 ) {
				if ( (*( p)) > -33 ) {
					if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -62 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -30 ) {
			if ( (*( p)) < -17 ) {
				if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -29 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto tr1609;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
tr1609:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 397 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1883;
st1883:
	if ( ++( p) == ( pe) )
		goto _test_eof1883;
case 1883:
#line 28710 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -29 ) {
			if ( (*( p)) < -62 ) {
				if ( (*( p)) <= -63 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -33 ) {
				if ( (*( p)) > -31 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -32 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -29 ) {
			if ( (*( p)) < -17 ) {
				if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1345;
			} else if ( _widec >= 896 )
				goto st1344;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1344;
			} else if ( _widec >= 1008 )
				goto st1353;
		} else
			goto st1346;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1344;
			} else if ( _widec >= 1038 )
				goto tr1609;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else if ( _widec >= 1089 )
				goto tr1609;
		} else
			goto tr1609;
	} else
		goto tr1609;
	goto tr2402;
st1346:
	if ( ++( p) == ( pe) )
		goto _test_eof1346;
case 1346:
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -30 ) {
			if ( (*( p)) < -64 ) {
				if ( (*( p)) <= -65 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -63 ) {
				if ( (*( p)) > -33 ) {
					if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -62 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -30 ) {
			if ( (*( p)) < -17 ) {
				if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -29 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto st1345;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1347:
	if ( ++( p) == ( pe) )
		goto _test_eof1347;
case 1347:
	_widec = (*( p));
	if ( (*( p)) < -11 ) {
		if ( (*( p)) < -32 ) {
			if ( (*( p)) < -98 ) {
				if ( (*( p)) > -100 ) {
					if ( -99 <= (*( p)) && (*( p)) <= -99 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -65 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -31 ) {
			if ( (*( p)) < -28 ) {
				if ( (*( p)) > -30 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -30 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -18 ) {
				if ( (*( p)) > -17 ) {
					if ( -16 <= (*( p)) && (*( p)) <= -12 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -17 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -1 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( (*( p)) > 8 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 1 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 925: goto st1348;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto st1345;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1348:
	if ( ++( p) == ( pe) )
		goto _test_eof1348;
case 1348:
	_widec = (*( p));
	if ( (*( p)) < -11 ) {
		if ( (*( p)) < -32 ) {
			if ( (*( p)) < -82 ) {
				if ( (*( p)) > -84 ) {
					if ( -83 <= (*( p)) && (*( p)) <= -83 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -65 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -31 ) {
			if ( (*( p)) < -28 ) {
				if ( (*( p)) > -30 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -30 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -18 ) {
				if ( (*( p)) > -17 ) {
					if ( -16 <= (*( p)) && (*( p)) <= -12 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -17 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -1 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( (*( p)) > 8 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 1 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 941: goto st1344;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto tr1609;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1349:
	if ( ++( p) == ( pe) )
		goto _test_eof1349;
case 1349:
	_widec = (*( p));
	if ( (*( p)) < -11 ) {
		if ( (*( p)) < -32 ) {
			if ( (*( p)) < -127 ) {
				if ( (*( p)) <= -128 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -65 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -31 ) {
			if ( (*( p)) < -28 ) {
				if ( (*( p)) > -30 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -30 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -18 ) {
				if ( (*( p)) > -17 ) {
					if ( -16 <= (*( p)) && (*( p)) <= -12 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -17 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -1 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( (*( p)) > 8 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 1 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 896: goto st1350;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 897 )
				goto st1345;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1350:
	if ( ++( p) == ( pe) )
		goto _test_eof1350;
case 1350:
	_widec = (*( p));
	if ( (*( p)) < -17 ) {
		if ( (*( p)) < -99 ) {
			if ( (*( p)) < -120 ) {
				if ( (*( p)) > -126 ) {
					if ( -125 <= (*( p)) && (*( p)) <= -121 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -111 ) {
				if ( (*( p)) > -109 ) {
					if ( -108 <= (*( p)) && (*( p)) <= -100 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -110 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -65 ) {
			if ( (*( p)) < -32 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -31 ) {
				if ( (*( p)) < -29 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -17 ) {
		if ( (*( p)) < 43 ) {
			if ( (*( p)) < 1 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 8 ) {
				if ( (*( p)) < 33 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 33 ) {
					if ( 39 <= (*( p)) && (*( p)) <= 39 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 43 ) {
			if ( (*( p)) < 65 ) {
				if ( (*( p)) > 47 ) {
					if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 45 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 90 ) {
				if ( (*( p)) < 97 ) {
					if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 992 ) {
		if ( _widec < 914 ) {
			if ( _widec < 899 ) {
				if ( 896 <= _widec && _widec <= 898 )
					goto st1344;
			} else if ( _widec > 903 ) {
				if ( 904 <= _widec && _widec <= 913 )
					goto st1344;
			} else
				goto tr1609;
		} else if ( _widec > 915 ) {
			if ( _widec < 925 ) {
				if ( 916 <= _widec && _widec <= 924 )
					goto st1344;
			} else if ( _widec > 959 ) {
				if ( _widec > 961 ) {
					if ( 962 <= _widec && _widec <= 991 )
						goto st1345;
				} else if ( _widec >= 960 )
					goto st1344;
			} else
				goto tr1609;
		} else
			goto tr1609;
	} else if ( _widec > 1006 ) {
		if ( _widec < 1038 ) {
			if ( _widec < 1013 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec > 1023 ) {
				if ( 1025 <= _widec && _widec <= 1032 )
					goto tr1609;
			} else
				goto st1344;
		} else if ( _widec > 1055 ) {
			if ( _widec < 1072 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1344;
			} else if ( _widec > 1081 ) {
				if ( _widec > 1114 ) {
					if ( 1121 <= _widec && _widec <= 1146 )
						goto tr1609;
				} else if ( _widec >= 1089 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto tr1609;
	} else
		goto st1346;
	goto tr234;
st1351:
	if ( ++( p) == ( pe) )
		goto _test_eof1351;
case 1351:
	_widec = (*( p));
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) < -67 ) {
				if ( (*( p)) > -69 ) {
					if ( -68 <= (*( p)) && (*( p)) <= -68 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -67 ) {
				if ( (*( p)) > -65 ) {
					if ( -64 <= (*( p)) && (*( p)) <= -63 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -66 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -33 ) {
			if ( (*( p)) < -29 ) {
				if ( (*( p)) > -31 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -32 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -29 ) {
				if ( (*( p)) > -18 ) {
					if ( -17 <= (*( p)) && (*( p)) <= -17 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -28 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 43 ) {
			if ( (*( p)) < 14 ) {
				if ( (*( p)) > -1 ) {
					if ( 1 <= (*( p)) && (*( p)) <= 8 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -11 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 31 ) {
				if ( (*( p)) > 33 ) {
					if ( 39 <= (*( p)) && (*( p)) <= 39 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 33 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 43 ) {
			if ( (*( p)) < 65 ) {
				if ( (*( p)) > 47 ) {
					if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 45 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 90 ) {
				if ( (*( p)) < 97 ) {
					if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 956: goto st1352;
		case 957: goto st1354;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto st1345;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1352:
	if ( ++( p) == ( pe) )
		goto _test_eof1352;
case 1352:
	_widec = (*( p));
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -64 ) {
			if ( (*( p)) < -118 ) {
				if ( (*( p)) > -120 ) {
					if ( -119 <= (*( p)) && (*( p)) <= -119 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -68 ) {
				if ( (*( p)) > -67 ) {
					if ( -66 <= (*( p)) && (*( p)) <= -65 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -67 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -63 ) {
			if ( (*( p)) < -30 ) {
				if ( (*( p)) > -33 ) {
					if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -62 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -30 ) {
				if ( (*( p)) < -28 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > -18 ) {
					if ( -17 <= (*( p)) && (*( p)) <= -17 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 43 ) {
			if ( (*( p)) < 14 ) {
				if ( (*( p)) > -1 ) {
					if ( 1 <= (*( p)) && (*( p)) <= 8 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -11 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 31 ) {
				if ( (*( p)) > 33 ) {
					if ( 39 <= (*( p)) && (*( p)) <= 39 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 33 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 43 ) {
			if ( (*( p)) < 65 ) {
				if ( (*( p)) > 47 ) {
					if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 45 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 90 ) {
				if ( (*( p)) < 97 ) {
					if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 905: goto st1344;
		case 957: goto st1344;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto tr1609;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1353:
	if ( ++( p) == ( pe) )
		goto _test_eof1353;
case 1353:
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -30 ) {
			if ( (*( p)) < -64 ) {
				if ( (*( p)) <= -65 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -63 ) {
				if ( (*( p)) > -33 ) {
					if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -62 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -30 ) {
			if ( (*( p)) < -17 ) {
				if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -29 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto st1346;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1354:
	if ( ++( p) == ( pe) )
		goto _test_eof1354;
case 1354:
	_widec = (*( p));
	if ( (*( p)) < -17 ) {
		if ( (*( p)) < -92 ) {
			if ( (*( p)) < -98 ) {
				if ( (*( p)) > -100 ) {
					if ( -99 <= (*( p)) && (*( p)) <= -99 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -97 ) {
				if ( (*( p)) < -95 ) {
					if ( -96 <= (*( p)) && (*( p)) <= -96 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > -94 ) {
					if ( -93 <= (*( p)) && (*( p)) <= -93 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -65 ) {
			if ( (*( p)) < -32 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -31 ) {
				if ( (*( p)) < -29 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -17 ) {
		if ( (*( p)) < 43 ) {
			if ( (*( p)) < 1 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 8 ) {
				if ( (*( p)) < 33 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 33 ) {
					if ( 39 <= (*( p)) && (*( p)) <= 39 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 43 ) {
			if ( (*( p)) < 65 ) {
				if ( (*( p)) > 47 ) {
					if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 45 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 90 ) {
				if ( (*( p)) < 97 ) {
					if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 925: goto st1344;
		case 928: goto st1344;
		case 931: goto st1344;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto tr1609;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1355:
	if ( ++( p) == ( pe) )
		goto _test_eof1355;
case 1355:
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -30 ) {
			if ( (*( p)) < -64 ) {
				if ( (*( p)) <= -65 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -63 ) {
				if ( (*( p)) > -33 ) {
					if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -62 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -30 ) {
			if ( (*( p)) < -17 ) {
				if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -29 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto tr1601;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
tr1601:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 397 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1884;
st1884:
	if ( ++( p) == ( pe) )
		goto _test_eof1884;
case 1884:
#line 31062 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -29 ) {
			if ( (*( p)) < -62 ) {
				if ( (*( p)) <= -63 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -33 ) {
				if ( (*( p)) > -31 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -32 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -29 ) {
			if ( (*( p)) < -17 ) {
				if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1357;
		case 995: goto st1359;
		case 1007: goto st1361;
		case 1057: goto st1343;
		case 1063: goto st1365;
		case 1067: goto st1343;
		case 1119: goto st1343;
		case 1151: goto tr1601;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1355;
			} else if ( _widec >= 896 )
				goto st1344;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1344;
			} else if ( _widec >= 1008 )
				goto st1364;
		} else
			goto st1356;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1343;
			} else if ( _widec >= 1038 )
				goto tr1601;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1601;
			} else if ( _widec >= 1089 )
				goto tr1601;
		} else
			goto tr1601;
	} else
		goto tr1601;
	goto tr2402;
st1356:
	if ( ++( p) == ( pe) )
		goto _test_eof1356;
case 1356:
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -30 ) {
			if ( (*( p)) < -64 ) {
				if ( (*( p)) <= -65 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -63 ) {
				if ( (*( p)) > -33 ) {
					if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -62 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -30 ) {
			if ( (*( p)) < -17 ) {
				if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -29 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto st1355;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1357:
	if ( ++( p) == ( pe) )
		goto _test_eof1357;
case 1357:
	_widec = (*( p));
	if ( (*( p)) < -11 ) {
		if ( (*( p)) < -32 ) {
			if ( (*( p)) < -98 ) {
				if ( (*( p)) > -100 ) {
					if ( -99 <= (*( p)) && (*( p)) <= -99 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -65 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -31 ) {
			if ( (*( p)) < -28 ) {
				if ( (*( p)) > -30 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -30 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -18 ) {
				if ( (*( p)) > -17 ) {
					if ( -16 <= (*( p)) && (*( p)) <= -12 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -17 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -1 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( (*( p)) > 8 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 1 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 925: goto st1358;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto st1355;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1358:
	if ( ++( p) == ( pe) )
		goto _test_eof1358;
case 1358:
	_widec = (*( p));
	if ( (*( p)) < -11 ) {
		if ( (*( p)) < -32 ) {
			if ( (*( p)) < -82 ) {
				if ( (*( p)) > -84 ) {
					if ( -83 <= (*( p)) && (*( p)) <= -83 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -65 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -31 ) {
			if ( (*( p)) < -28 ) {
				if ( (*( p)) > -30 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -30 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -18 ) {
				if ( (*( p)) > -17 ) {
					if ( -16 <= (*( p)) && (*( p)) <= -12 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -17 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -1 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( (*( p)) > 8 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 1 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 941: goto st1343;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto tr1601;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1359:
	if ( ++( p) == ( pe) )
		goto _test_eof1359;
case 1359:
	_widec = (*( p));
	if ( (*( p)) < -11 ) {
		if ( (*( p)) < -32 ) {
			if ( (*( p)) < -127 ) {
				if ( (*( p)) <= -128 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -65 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -31 ) {
			if ( (*( p)) < -28 ) {
				if ( (*( p)) > -30 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -30 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -18 ) {
				if ( (*( p)) > -17 ) {
					if ( -16 <= (*( p)) && (*( p)) <= -12 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -17 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -1 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( (*( p)) > 8 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 1 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 896: goto st1360;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 897 )
				goto st1355;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1360:
	if ( ++( p) == ( pe) )
		goto _test_eof1360;
case 1360:
	_widec = (*( p));
	if ( (*( p)) < -17 ) {
		if ( (*( p)) < -99 ) {
			if ( (*( p)) < -120 ) {
				if ( (*( p)) > -126 ) {
					if ( -125 <= (*( p)) && (*( p)) <= -121 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -111 ) {
				if ( (*( p)) > -109 ) {
					if ( -108 <= (*( p)) && (*( p)) <= -100 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -110 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -65 ) {
			if ( (*( p)) < -32 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -31 ) {
				if ( (*( p)) < -29 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -17 ) {
		if ( (*( p)) < 43 ) {
			if ( (*( p)) < 1 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 8 ) {
				if ( (*( p)) < 33 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 33 ) {
					if ( 39 <= (*( p)) && (*( p)) <= 39 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 43 ) {
			if ( (*( p)) < 65 ) {
				if ( (*( p)) > 47 ) {
					if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 45 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 90 ) {
				if ( (*( p)) < 97 ) {
					if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 992 ) {
		if ( _widec < 914 ) {
			if ( _widec < 899 ) {
				if ( 896 <= _widec && _widec <= 898 )
					goto st1343;
			} else if ( _widec > 903 ) {
				if ( 904 <= _widec && _widec <= 913 )
					goto st1343;
			} else
				goto tr1601;
		} else if ( _widec > 915 ) {
			if ( _widec < 925 ) {
				if ( 916 <= _widec && _widec <= 924 )
					goto st1343;
			} else if ( _widec > 959 ) {
				if ( _widec > 961 ) {
					if ( 962 <= _widec && _widec <= 991 )
						goto st1345;
				} else if ( _widec >= 960 )
					goto st1344;
			} else
				goto tr1601;
		} else
			goto tr1601;
	} else if ( _widec > 1006 ) {
		if ( _widec < 1038 ) {
			if ( _widec < 1013 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec > 1023 ) {
				if ( 1025 <= _widec && _widec <= 1032 )
					goto tr1609;
			} else
				goto st1344;
		} else if ( _widec > 1055 ) {
			if ( _widec < 1072 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1344;
			} else if ( _widec > 1081 ) {
				if ( _widec > 1114 ) {
					if ( 1121 <= _widec && _widec <= 1146 )
						goto tr1609;
				} else if ( _widec >= 1089 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto tr1609;
	} else
		goto st1346;
	goto tr234;
st1361:
	if ( ++( p) == ( pe) )
		goto _test_eof1361;
case 1361:
	_widec = (*( p));
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) < -67 ) {
				if ( (*( p)) > -69 ) {
					if ( -68 <= (*( p)) && (*( p)) <= -68 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -67 ) {
				if ( (*( p)) > -65 ) {
					if ( -64 <= (*( p)) && (*( p)) <= -63 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -66 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -33 ) {
			if ( (*( p)) < -29 ) {
				if ( (*( p)) > -31 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -32 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -29 ) {
				if ( (*( p)) > -18 ) {
					if ( -17 <= (*( p)) && (*( p)) <= -17 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -28 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 43 ) {
			if ( (*( p)) < 14 ) {
				if ( (*( p)) > -1 ) {
					if ( 1 <= (*( p)) && (*( p)) <= 8 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -11 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 31 ) {
				if ( (*( p)) > 33 ) {
					if ( 39 <= (*( p)) && (*( p)) <= 39 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 33 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 43 ) {
			if ( (*( p)) < 65 ) {
				if ( (*( p)) > 47 ) {
					if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 45 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 90 ) {
				if ( (*( p)) < 97 ) {
					if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 956: goto st1362;
		case 957: goto st1363;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto st1355;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1362:
	if ( ++( p) == ( pe) )
		goto _test_eof1362;
case 1362:
	_widec = (*( p));
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -64 ) {
			if ( (*( p)) < -118 ) {
				if ( (*( p)) > -120 ) {
					if ( -119 <= (*( p)) && (*( p)) <= -119 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -68 ) {
				if ( (*( p)) > -67 ) {
					if ( -66 <= (*( p)) && (*( p)) <= -65 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -67 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -63 ) {
			if ( (*( p)) < -30 ) {
				if ( (*( p)) > -33 ) {
					if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -62 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -30 ) {
				if ( (*( p)) < -28 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > -18 ) {
					if ( -17 <= (*( p)) && (*( p)) <= -17 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 43 ) {
			if ( (*( p)) < 14 ) {
				if ( (*( p)) > -1 ) {
					if ( 1 <= (*( p)) && (*( p)) <= 8 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -11 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 31 ) {
				if ( (*( p)) > 33 ) {
					if ( 39 <= (*( p)) && (*( p)) <= 39 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 33 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 43 ) {
			if ( (*( p)) < 65 ) {
				if ( (*( p)) > 47 ) {
					if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 45 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 90 ) {
				if ( (*( p)) < 97 ) {
					if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 905: goto st1343;
		case 957: goto st1343;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto tr1601;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1363:
	if ( ++( p) == ( pe) )
		goto _test_eof1363;
case 1363:
	_widec = (*( p));
	if ( (*( p)) < -17 ) {
		if ( (*( p)) < -92 ) {
			if ( (*( p)) < -98 ) {
				if ( (*( p)) > -100 ) {
					if ( -99 <= (*( p)) && (*( p)) <= -99 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -97 ) {
				if ( (*( p)) < -95 ) {
					if ( -96 <= (*( p)) && (*( p)) <= -96 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > -94 ) {
					if ( -93 <= (*( p)) && (*( p)) <= -93 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -65 ) {
			if ( (*( p)) < -32 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -64 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -31 ) {
				if ( (*( p)) < -29 ) {
					if ( -30 <= (*( p)) && (*( p)) <= -30 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -17 ) {
		if ( (*( p)) < 43 ) {
			if ( (*( p)) < 1 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 8 ) {
				if ( (*( p)) < 33 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 33 ) {
					if ( 39 <= (*( p)) && (*( p)) <= 39 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 43 ) {
			if ( (*( p)) < 65 ) {
				if ( (*( p)) > 47 ) {
					if ( 48 <= (*( p)) && (*( p)) <= 57 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 45 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 90 ) {
				if ( (*( p)) < 97 ) {
					if ( 95 <= (*( p)) && (*( p)) <= 95 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 925: goto st1343;
		case 928: goto st1343;
		case 931: goto st1343;
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto tr1601;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1364:
	if ( ++( p) == ( pe) )
		goto _test_eof1364;
case 1364:
	_widec = (*( p));
	if ( (*( p)) < 1 ) {
		if ( (*( p)) < -30 ) {
			if ( (*( p)) < -64 ) {
				if ( (*( p)) <= -65 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -63 ) {
				if ( (*( p)) > -33 ) {
					if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -62 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -30 ) {
			if ( (*( p)) < -17 ) {
				if ( (*( p)) > -29 ) {
					if ( -28 <= (*( p)) && (*( p)) <= -18 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -29 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -17 ) {
				if ( (*( p)) > -12 ) {
					if ( -11 <= (*( p)) && (*( p)) <= -1 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -16 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 8 ) {
		if ( (*( p)) < 45 ) {
			if ( (*( p)) < 33 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 33 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 47 ) {
			if ( (*( p)) < 95 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 95 ) {
				if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1347;
		case 995: goto st1349;
		case 1007: goto st1351;
		case 1057: goto st1344;
		case 1063: goto st1344;
		case 1067: goto st1344;
		case 1119: goto st1344;
		case 1151: goto tr1609;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1344;
			} else if ( _widec >= 896 )
				goto st1356;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1353;
			} else if ( _widec >= 992 )
				goto st1346;
		} else
			goto st1345;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1609;
			} else if ( _widec >= 1025 )
				goto tr1609;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1609;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1609;
			} else
				goto tr1609;
		} else
			goto st1344;
	} else
		goto st1344;
	goto tr234;
st1365:
	if ( ++( p) == ( pe) )
		goto _test_eof1365;
case 1365:
	_widec = (*( p));
	if ( (*( p)) < 33 ) {
		if ( (*( p)) < -28 ) {
			if ( (*( p)) < -32 ) {
				if ( (*( p)) > -63 ) {
					if ( -62 <= (*( p)) && (*( p)) <= -33 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -31 ) {
				if ( (*( p)) > -30 ) {
					if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -30 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -18 ) {
			if ( (*( p)) < -11 ) {
				if ( (*( p)) > -17 ) {
					if ( -16 <= (*( p)) && (*( p)) <= -12 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= -17 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -1 ) {
				if ( (*( p)) > 8 ) {
					if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 1 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > 33 ) {
		if ( (*( p)) < 95 ) {
			if ( (*( p)) < 45 ) {
				if ( (*( p)) > 39 ) {
					if ( 43 <= (*( p)) && (*( p)) <= 43 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 39 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 47 ) {
				if ( (*( p)) > 57 ) {
					if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 48 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 95 ) {
			if ( (*( p)) < 101 ) {
				if ( (*( p)) > 99 ) {
					if ( 100 <= (*( p)) && (*( p)) <= 100 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) >= 97 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 114 ) {
				if ( (*( p)) < 116 ) {
					if ( 115 <= (*( p)) && (*( p)) <= 115 ) {
						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( p)) > 122 ) {
					if ( 127 <= (*( p)) )
 {						_widec = (short)(640 + ((*( p)) - -128));
						if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1357;
		case 995: goto st1359;
		case 1007: goto st1361;
		case 1057: goto st1343;
		case 1063: goto st1365;
		case 1067: goto st1343;
		case 1119: goto st1343;
		case 1124: goto st1343;
		case 1139: goto st1343;
		case 1151: goto tr1601;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1355;
			} else if ( _widec >= 896 )
				goto st1344;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1344;
			} else if ( _widec >= 1008 )
				goto st1364;
		} else
			goto st1356;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1343;
			} else if ( _widec >= 1038 )
				goto tr1601;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1601;
			} else if ( _widec >= 1089 )
				goto tr1601;
		} else
			goto tr1601;
	} else
		goto tr1601;
	goto tr234;
tr2395:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1366;
st1366:
	if ( ++( p) == ( pe) )
		goto _test_eof1366;
case 1366:
#line 33430 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1342;
	goto tr241;
tr2396:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1367;
st1367:
	if ( ++( p) == ( pe) )
		goto _test_eof1367;
case 1367:
#line 33449 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) < -99 ) {
		if ( (*( p)) <= -100 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -99 ) {
		if ( -98 <= (*( p)) && (*( p)) <= -65 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( _widec == 925 )
		goto st1368;
	if ( 896 <= _widec && _widec <= 959 )
		goto st1342;
	goto tr241;
st1368:
	if ( ++( p) == ( pe) )
		goto _test_eof1368;
case 1368:
	_widec = (*( p));
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( _widec > 940 ) {
		if ( 942 <= _widec && _widec <= 959 )
			goto st1343;
	} else if ( _widec >= 896 )
		goto st1343;
	goto tr241;
tr2397:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1369;
st1369:
	if ( ++( p) == ( pe) )
		goto _test_eof1369;
case 1369:
#line 33508 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) > -128 ) {
		if ( -127 <= (*( p)) && (*( p)) <= -65 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( _widec == 896 )
		goto st1370;
	if ( 897 <= _widec && _widec <= 959 )
		goto st1342;
	goto tr241;
st1370:
	if ( ++( p) == ( pe) )
		goto _test_eof1370;
case 1370:
	_widec = (*( p));
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( _widec < 914 ) {
		if ( 899 <= _widec && _widec <= 903 )
			goto st1343;
	} else if ( _widec > 915 ) {
		if ( 925 <= _widec && _widec <= 959 )
			goto st1343;
	} else
		goto st1343;
	goto tr241;
tr2398:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1371;
st1371:
	if ( ++( p) == ( pe) )
		goto _test_eof1371;
case 1371:
#line 33570 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) < -68 ) {
		if ( (*( p)) <= -69 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -68 ) {
		if ( (*( p)) > -67 ) {
			if ( -66 <= (*( p)) && (*( p)) <= -65 ) {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) >= -67 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 956: goto st1372;
		case 957: goto st1373;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1342;
	goto tr241;
st1372:
	if ( ++( p) == ( pe) )
		goto _test_eof1372;
case 1372:
	_widec = (*( p));
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( _widec < 906 ) {
		if ( 896 <= _widec && _widec <= 904 )
			goto st1343;
	} else if ( _widec > 956 ) {
		if ( 958 <= _widec && _widec <= 959 )
			goto st1343;
	} else
		goto st1343;
	goto tr241;
st1373:
	if ( ++( p) == ( pe) )
		goto _test_eof1373;
case 1373:
	_widec = (*( p));
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 ) {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) >= -95 ) {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( _widec < 926 ) {
		if ( 896 <= _widec && _widec <= 924 )
			goto st1343;
	} else if ( _widec > 927 ) {
		if ( _widec > 930 ) {
			if ( 932 <= _widec && _widec <= 959 )
				goto st1343;
		} else if ( _widec >= 929 )
			goto st1343;
	} else
		goto st1343;
	goto tr241;
tr2399:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1374;
st1374:
	if ( ++( p) == ( pe) )
		goto _test_eof1374;
case 1374:
#line 33692 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1366;
	goto tr241;
tr2401:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1375;
st1375:
	if ( ++( p) == ( pe) )
		goto _test_eof1375;
case 1375:
#line 33711 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -30 ) {
			if ( (*( p)) > -33 ) {
				if ( -32 <= (*( p)) && (*( p)) <= -31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) >= -62 ) {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > -30 ) {
			if ( (*( p)) < -28 ) {
				if ( -29 <= (*( p)) && (*( p)) <= -29 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > -18 ) {
				if ( -17 <= (*( p)) && (*( p)) <= -17 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 48 ) {
			if ( (*( p)) > 8 ) {
				if ( 14 <= (*( p)) && (*( p)) <= 31 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) >= 1 ) {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( p)) > 57 ) {
			if ( (*( p)) < 97 ) {
				if ( 65 <= (*( p)) && (*( p)) <= 90 ) {
					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( p)) > 122 ) {
				if ( 127 <= (*( p)) )
 {					_widec = (short)(640 + ((*( p)) - -128));
					if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( p)) - -128));
				if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( p)) - -128));
			if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1367;
		case 995: goto st1369;
		case 1007: goto st1371;
		case 1151: goto st1343;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( 962 <= _widec && _widec <= 991 )
				goto st1342;
		} else if ( _widec > 1006 ) {
			if ( 1008 <= _widec && _widec <= 1012 )
				goto st1374;
		} else
			goto st1366;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( 1038 <= _widec && _widec <= 1055 )
				goto st1343;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto st1343;
			} else if ( _widec >= 1089 )
				goto st1343;
		} else
			goto st1343;
	} else
		goto st1343;
	goto tr241;
tr1628:
#line 609 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1885;
tr1634:
#line 602 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_rewind();
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1885;
tr2403:
#line 609 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1885;
tr2404:
#line 607 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1885;
tr2408:
#line 609 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1885;
st1885:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1885;
case 1885:
#line 1 "NONE"
	{( ts) = ( p);}
#line 33870 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2404;
		case 10: goto tr2405;
		case 60: goto tr2406;
		case 91: goto tr2407;
	}
	goto tr2403;
tr2405:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1886;
st1886:
	if ( ++( p) == ( pe) )
		goto _test_eof1886;
case 1886:
#line 33886 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto st1376;
		case 91: goto st1382;
	}
	goto tr2408;
st1376:
	if ( ++( p) == ( pe) )
		goto _test_eof1376;
case 1376:
	if ( (*( p)) == 47 )
		goto st1377;
	goto tr1628;
st1377:
	if ( ++( p) == ( pe) )
		goto _test_eof1377;
case 1377:
	switch( (*( p)) ) {
		case 67: goto st1378;
		case 99: goto st1378;
	}
	goto tr1628;
st1378:
	if ( ++( p) == ( pe) )
		goto _test_eof1378;
case 1378:
	switch( (*( p)) ) {
		case 79: goto st1379;
		case 111: goto st1379;
	}
	goto tr1628;
st1379:
	if ( ++( p) == ( pe) )
		goto _test_eof1379;
case 1379:
	switch( (*( p)) ) {
		case 68: goto st1380;
		case 100: goto st1380;
	}
	goto tr1628;
st1380:
	if ( ++( p) == ( pe) )
		goto _test_eof1380;
case 1380:
	switch( (*( p)) ) {
		case 69: goto st1381;
		case 101: goto st1381;
	}
	goto tr1628;
st1381:
	if ( ++( p) == ( pe) )
		goto _test_eof1381;
case 1381:
	if ( (*( p)) == 62 )
		goto tr1634;
	goto tr1628;
st1382:
	if ( ++( p) == ( pe) )
		goto _test_eof1382;
case 1382:
	if ( (*( p)) == 47 )
		goto st1383;
	goto tr1628;
st1383:
	if ( ++( p) == ( pe) )
		goto _test_eof1383;
case 1383:
	switch( (*( p)) ) {
		case 67: goto st1384;
		case 99: goto st1384;
	}
	goto tr1628;
st1384:
	if ( ++( p) == ( pe) )
		goto _test_eof1384;
case 1384:
	switch( (*( p)) ) {
		case 79: goto st1385;
		case 111: goto st1385;
	}
	goto tr1628;
st1385:
	if ( ++( p) == ( pe) )
		goto _test_eof1385;
case 1385:
	switch( (*( p)) ) {
		case 68: goto st1386;
		case 100: goto st1386;
	}
	goto tr1628;
st1386:
	if ( ++( p) == ( pe) )
		goto _test_eof1386;
case 1386:
	switch( (*( p)) ) {
		case 69: goto st1387;
		case 101: goto st1387;
	}
	goto tr1628;
st1387:
	if ( ++( p) == ( pe) )
		goto _test_eof1387;
case 1387:
	if ( (*( p)) == 93 )
		goto tr1634;
	goto tr1628;
tr2406:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1887;
st1887:
	if ( ++( p) == ( pe) )
		goto _test_eof1887;
case 1887:
#line 34000 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1377;
	goto tr2408;
tr2407:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1888;
st1888:
	if ( ++( p) == ( pe) )
		goto _test_eof1888;
case 1888:
#line 34012 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1383;
	goto tr2408;
tr1640:
#line 622 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1889;
tr1649:
#line 615 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_rewind();
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1889;
tr2411:
#line 622 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1889;
tr2412:
#line 620 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1889;
tr2416:
#line 622 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1889;
st1889:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1889;
case 1889:
#line 1 "NONE"
	{( ts) = ( p);}
#line 34053 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2412;
		case 10: goto tr2413;
		case 60: goto tr2414;
		case 91: goto tr2415;
	}
	goto tr2411;
tr2413:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1890;
st1890:
	if ( ++( p) == ( pe) )
		goto _test_eof1890;
case 1890:
#line 34069 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto st1388;
		case 91: goto st1397;
	}
	goto tr2416;
st1388:
	if ( ++( p) == ( pe) )
		goto _test_eof1388;
case 1388:
	if ( (*( p)) == 47 )
		goto st1389;
	goto tr1640;
st1389:
	if ( ++( p) == ( pe) )
		goto _test_eof1389;
case 1389:
	switch( (*( p)) ) {
		case 78: goto st1390;
		case 110: goto st1390;
	}
	goto tr1640;
st1390:
	if ( ++( p) == ( pe) )
		goto _test_eof1390;
case 1390:
	switch( (*( p)) ) {
		case 79: goto st1391;
		case 111: goto st1391;
	}
	goto tr1640;
st1391:
	if ( ++( p) == ( pe) )
		goto _test_eof1391;
case 1391:
	switch( (*( p)) ) {
		case 68: goto st1392;
		case 100: goto st1392;
	}
	goto tr1640;
st1392:
	if ( ++( p) == ( pe) )
		goto _test_eof1392;
case 1392:
	switch( (*( p)) ) {
		case 84: goto st1393;
		case 116: goto st1393;
	}
	goto tr1640;
st1393:
	if ( ++( p) == ( pe) )
		goto _test_eof1393;
case 1393:
	switch( (*( p)) ) {
		case 69: goto st1394;
		case 101: goto st1394;
	}
	goto tr1640;
st1394:
	if ( ++( p) == ( pe) )
		goto _test_eof1394;
case 1394:
	switch( (*( p)) ) {
		case 88: goto st1395;
		case 120: goto st1395;
	}
	goto tr1640;
st1395:
	if ( ++( p) == ( pe) )
		goto _test_eof1395;
case 1395:
	switch( (*( p)) ) {
		case 84: goto st1396;
		case 116: goto st1396;
	}
	goto tr1640;
st1396:
	if ( ++( p) == ( pe) )
		goto _test_eof1396;
case 1396:
	if ( (*( p)) == 62 )
		goto tr1649;
	goto tr1640;
st1397:
	if ( ++( p) == ( pe) )
		goto _test_eof1397;
case 1397:
	if ( (*( p)) == 47 )
		goto st1398;
	goto tr1640;
st1398:
	if ( ++( p) == ( pe) )
		goto _test_eof1398;
case 1398:
	switch( (*( p)) ) {
		case 78: goto st1399;
		case 110: goto st1399;
	}
	goto tr1640;
st1399:
	if ( ++( p) == ( pe) )
		goto _test_eof1399;
case 1399:
	switch( (*( p)) ) {
		case 79: goto st1400;
		case 111: goto st1400;
	}
	goto tr1640;
st1400:
	if ( ++( p) == ( pe) )
		goto _test_eof1400;
case 1400:
	switch( (*( p)) ) {
		case 68: goto st1401;
		case 100: goto st1401;
	}
	goto tr1640;
st1401:
	if ( ++( p) == ( pe) )
		goto _test_eof1401;
case 1401:
	switch( (*( p)) ) {
		case 84: goto st1402;
		case 116: goto st1402;
	}
	goto tr1640;
st1402:
	if ( ++( p) == ( pe) )
		goto _test_eof1402;
case 1402:
	switch( (*( p)) ) {
		case 69: goto st1403;
		case 101: goto st1403;
	}
	goto tr1640;
st1403:
	if ( ++( p) == ( pe) )
		goto _test_eof1403;
case 1403:
	switch( (*( p)) ) {
		case 88: goto st1404;
		case 120: goto st1404;
	}
	goto tr1640;
st1404:
	if ( ++( p) == ( pe) )
		goto _test_eof1404;
case 1404:
	switch( (*( p)) ) {
		case 84: goto st1405;
		case 116: goto st1405;
	}
	goto tr1640;
st1405:
	if ( ++( p) == ( pe) )
		goto _test_eof1405;
case 1405:
	if ( (*( p)) == 93 )
		goto tr1649;
	goto tr1640;
tr2414:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1891;
st1891:
	if ( ++( p) == ( pe) )
		goto _test_eof1891;
case 1891:
#line 34237 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1389;
	goto tr2416;
tr2415:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1892;
st1892:
	if ( ++( p) == ( pe) )
		goto _test_eof1892;
case 1892:
#line 34249 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1398;
	goto tr2416;
tr1658:
#line 681 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}}
	goto st1893;
tr1668:
#line 632 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_COLGROUP, { ts, te });
  }}
	goto st1893;
tr1676:
#line 675 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TABLE, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1893;
tr1680:
#line 653 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_TBODY, { ts, te });
  }}
	goto st1893;
tr1684:
#line 645 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_THEAD, { ts, te });
  }}
	goto st1893;
tr1685:
#line 666 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_TR, { ts, te });
  }}
	goto st1893;
tr1689:
#line 636 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1893;
tr1704:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 636 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1893;
tr1709:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 636 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1893;
tr1715:
#line 628 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1893;
tr1729:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 628 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1893;
tr1734:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 628 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1893;
tr1743:
#line 649 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1893;
tr1757:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 649 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1893;
tr1762:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 649 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1893;
tr1764:
#line 670 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TD, "td");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1893;goto st1659;}}
  }}
	goto st1893;
tr1778:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 670 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TD, "td");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1893;goto st1659;}}
  }}
	goto st1893;
tr1783:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 670 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TD, "td");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1893;goto st1659;}}
  }}
	goto st1893;
tr1785:
#line 657 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TH, "th");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1893;goto st1659;}}
  }}
	goto st1893;
tr1800:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 657 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TH, "th");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1893;goto st1659;}}
  }}
	goto st1893;
tr1805:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 657 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TH, "th");
    {
  size_t len = stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (top >= len) {
    g_debug("growing stack %zi", len + 16);
    stack.resize(len + 16, 0);
  }
{( (stack.data()))[( top)++] = 1893;goto st1659;}}
  }}
	goto st1893;
tr1809:
#line 641 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1893;
tr1823:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 641 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1893;
tr1828:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 641 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1893;
tr1830:
#line 662 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1893;
tr1844:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 662 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1893;
tr1849:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 662 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1893;
tr2419:
#line 681 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1893;
tr2422:
#line 681 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;}
	goto st1893;
st1893:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1893;
case 1893:
#line 1 "NONE"
	{( ts) = ( p);}
#line 34554 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto tr2420;
		case 91: goto tr2421;
	}
	goto tr2419;
tr2420:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1894;
st1894:
	if ( ++( p) == ( pe) )
		goto _test_eof1894;
case 1894:
#line 34568 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1406;
		case 67: goto st1429;
		case 84: goto st1457;
		case 99: goto st1429;
		case 116: goto st1457;
	}
	goto tr2422;
st1406:
	if ( ++( p) == ( pe) )
		goto _test_eof1406;
case 1406:
	switch( (*( p)) ) {
		case 67: goto st1407;
		case 84: goto st1415;
		case 99: goto st1407;
		case 116: goto st1415;
	}
	goto tr1658;
st1407:
	if ( ++( p) == ( pe) )
		goto _test_eof1407;
case 1407:
	switch( (*( p)) ) {
		case 79: goto st1408;
		case 111: goto st1408;
	}
	goto tr1658;
st1408:
	if ( ++( p) == ( pe) )
		goto _test_eof1408;
case 1408:
	switch( (*( p)) ) {
		case 76: goto st1409;
		case 108: goto st1409;
	}
	goto tr1658;
st1409:
	if ( ++( p) == ( pe) )
		goto _test_eof1409;
case 1409:
	switch( (*( p)) ) {
		case 71: goto st1410;
		case 103: goto st1410;
	}
	goto tr1658;
st1410:
	if ( ++( p) == ( pe) )
		goto _test_eof1410;
case 1410:
	switch( (*( p)) ) {
		case 82: goto st1411;
		case 114: goto st1411;
	}
	goto tr1658;
st1411:
	if ( ++( p) == ( pe) )
		goto _test_eof1411;
case 1411:
	switch( (*( p)) ) {
		case 79: goto st1412;
		case 111: goto st1412;
	}
	goto tr1658;
st1412:
	if ( ++( p) == ( pe) )
		goto _test_eof1412;
case 1412:
	switch( (*( p)) ) {
		case 85: goto st1413;
		case 117: goto st1413;
	}
	goto tr1658;
st1413:
	if ( ++( p) == ( pe) )
		goto _test_eof1413;
case 1413:
	switch( (*( p)) ) {
		case 80: goto st1414;
		case 112: goto st1414;
	}
	goto tr1658;
st1414:
	if ( ++( p) == ( pe) )
		goto _test_eof1414;
case 1414:
	if ( (*( p)) == 62 )
		goto tr1668;
	goto tr1658;
st1415:
	if ( ++( p) == ( pe) )
		goto _test_eof1415;
case 1415:
	switch( (*( p)) ) {
		case 65: goto st1416;
		case 66: goto st1420;
		case 72: goto st1424;
		case 82: goto st1428;
		case 97: goto st1416;
		case 98: goto st1420;
		case 104: goto st1424;
		case 114: goto st1428;
	}
	goto tr1658;
st1416:
	if ( ++( p) == ( pe) )
		goto _test_eof1416;
case 1416:
	switch( (*( p)) ) {
		case 66: goto st1417;
		case 98: goto st1417;
	}
	goto tr1658;
st1417:
	if ( ++( p) == ( pe) )
		goto _test_eof1417;
case 1417:
	switch( (*( p)) ) {
		case 76: goto st1418;
		case 108: goto st1418;
	}
	goto tr1658;
st1418:
	if ( ++( p) == ( pe) )
		goto _test_eof1418;
case 1418:
	switch( (*( p)) ) {
		case 69: goto st1419;
		case 101: goto st1419;
	}
	goto tr1658;
st1419:
	if ( ++( p) == ( pe) )
		goto _test_eof1419;
case 1419:
	if ( (*( p)) == 62 )
		goto tr1676;
	goto tr1658;
st1420:
	if ( ++( p) == ( pe) )
		goto _test_eof1420;
case 1420:
	switch( (*( p)) ) {
		case 79: goto st1421;
		case 111: goto st1421;
	}
	goto tr1658;
st1421:
	if ( ++( p) == ( pe) )
		goto _test_eof1421;
case 1421:
	switch( (*( p)) ) {
		case 68: goto st1422;
		case 100: goto st1422;
	}
	goto tr1658;
st1422:
	if ( ++( p) == ( pe) )
		goto _test_eof1422;
case 1422:
	switch( (*( p)) ) {
		case 89: goto st1423;
		case 121: goto st1423;
	}
	goto tr1658;
st1423:
	if ( ++( p) == ( pe) )
		goto _test_eof1423;
case 1423:
	if ( (*( p)) == 62 )
		goto tr1680;
	goto tr1658;
st1424:
	if ( ++( p) == ( pe) )
		goto _test_eof1424;
case 1424:
	switch( (*( p)) ) {
		case 69: goto st1425;
		case 101: goto st1425;
	}
	goto tr1658;
st1425:
	if ( ++( p) == ( pe) )
		goto _test_eof1425;
case 1425:
	switch( (*( p)) ) {
		case 65: goto st1426;
		case 97: goto st1426;
	}
	goto tr1658;
st1426:
	if ( ++( p) == ( pe) )
		goto _test_eof1426;
case 1426:
	switch( (*( p)) ) {
		case 68: goto st1427;
		case 100: goto st1427;
	}
	goto tr1658;
st1427:
	if ( ++( p) == ( pe) )
		goto _test_eof1427;
case 1427:
	if ( (*( p)) == 62 )
		goto tr1684;
	goto tr1658;
st1428:
	if ( ++( p) == ( pe) )
		goto _test_eof1428;
case 1428:
	if ( (*( p)) == 62 )
		goto tr1685;
	goto tr1658;
st1429:
	if ( ++( p) == ( pe) )
		goto _test_eof1429;
case 1429:
	switch( (*( p)) ) {
		case 79: goto st1430;
		case 111: goto st1430;
	}
	goto tr1658;
st1430:
	if ( ++( p) == ( pe) )
		goto _test_eof1430;
case 1430:
	switch( (*( p)) ) {
		case 76: goto st1431;
		case 108: goto st1431;
	}
	goto tr1658;
st1431:
	if ( ++( p) == ( pe) )
		goto _test_eof1431;
case 1431:
	switch( (*( p)) ) {
		case 9: goto st1432;
		case 32: goto st1432;
		case 62: goto tr1689;
		case 71: goto st1442;
		case 103: goto st1442;
	}
	goto tr1658;
tr1703:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1432;
tr1707:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1432;
st1432:
	if ( ++( p) == ( pe) )
		goto _test_eof1432;
case 1432:
#line 34826 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1432;
		case 32: goto st1432;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1691;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1691;
	} else
		goto tr1691;
	goto tr1658;
tr1691:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1433;
st1433:
	if ( ++( p) == ( pe) )
		goto _test_eof1433;
case 1433:
#line 34848 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1692;
		case 32: goto tr1692;
		case 61: goto tr1694;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1433;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1433;
	} else
		goto st1433;
	goto tr1658;
tr1692:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1434;
st1434:
	if ( ++( p) == ( pe) )
		goto _test_eof1434;
case 1434:
#line 34871 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1434;
		case 32: goto st1434;
		case 61: goto st1435;
	}
	goto tr1658;
tr1694:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1435;
st1435:
	if ( ++( p) == ( pe) )
		goto _test_eof1435;
case 1435:
#line 34886 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1435;
		case 32: goto st1435;
		case 34: goto st1436;
		case 39: goto st1439;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1699;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1699;
	} else
		goto tr1699;
	goto tr1658;
st1436:
	if ( ++( p) == ( pe) )
		goto _test_eof1436;
case 1436:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1700;
tr1700:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1437;
st1437:
	if ( ++( p) == ( pe) )
		goto _test_eof1437;
case 1437:
#line 34920 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1702;
	}
	goto st1437;
tr1702:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1438;
st1438:
	if ( ++( p) == ( pe) )
		goto _test_eof1438;
case 1438:
#line 34936 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1703;
		case 32: goto tr1703;
		case 62: goto tr1704;
	}
	goto tr1658;
st1439:
	if ( ++( p) == ( pe) )
		goto _test_eof1439;
case 1439:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1705;
tr1705:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1440;
st1440:
	if ( ++( p) == ( pe) )
		goto _test_eof1440;
case 1440:
#line 34961 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1702;
	}
	goto st1440;
tr1699:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1441;
st1441:
	if ( ++( p) == ( pe) )
		goto _test_eof1441;
case 1441:
#line 34977 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1707;
		case 32: goto tr1707;
		case 62: goto tr1709;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1441;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1441;
	} else
		goto st1441;
	goto tr1658;
st1442:
	if ( ++( p) == ( pe) )
		goto _test_eof1442;
case 1442:
	switch( (*( p)) ) {
		case 82: goto st1443;
		case 114: goto st1443;
	}
	goto tr1658;
st1443:
	if ( ++( p) == ( pe) )
		goto _test_eof1443;
case 1443:
	switch( (*( p)) ) {
		case 79: goto st1444;
		case 111: goto st1444;
	}
	goto tr1658;
st1444:
	if ( ++( p) == ( pe) )
		goto _test_eof1444;
case 1444:
	switch( (*( p)) ) {
		case 85: goto st1445;
		case 117: goto st1445;
	}
	goto tr1658;
st1445:
	if ( ++( p) == ( pe) )
		goto _test_eof1445;
case 1445:
	switch( (*( p)) ) {
		case 80: goto st1446;
		case 112: goto st1446;
	}
	goto tr1658;
st1446:
	if ( ++( p) == ( pe) )
		goto _test_eof1446;
case 1446:
	switch( (*( p)) ) {
		case 9: goto st1447;
		case 32: goto st1447;
		case 62: goto tr1715;
	}
	goto tr1658;
tr1728:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1447;
tr1732:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1447;
st1447:
	if ( ++( p) == ( pe) )
		goto _test_eof1447;
case 1447:
#line 35052 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1447;
		case 32: goto st1447;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1716;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1716;
	} else
		goto tr1716;
	goto tr1658;
tr1716:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1448;
st1448:
	if ( ++( p) == ( pe) )
		goto _test_eof1448;
case 1448:
#line 35074 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1717;
		case 32: goto tr1717;
		case 61: goto tr1719;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1448;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1448;
	} else
		goto st1448;
	goto tr1658;
tr1717:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1449;
st1449:
	if ( ++( p) == ( pe) )
		goto _test_eof1449;
case 1449:
#line 35097 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1449;
		case 32: goto st1449;
		case 61: goto st1450;
	}
	goto tr1658;
tr1719:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1450;
st1450:
	if ( ++( p) == ( pe) )
		goto _test_eof1450;
case 1450:
#line 35112 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1450;
		case 32: goto st1450;
		case 34: goto st1451;
		case 39: goto st1454;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1724;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1724;
	} else
		goto tr1724;
	goto tr1658;
st1451:
	if ( ++( p) == ( pe) )
		goto _test_eof1451;
case 1451:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1725;
tr1725:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1452;
st1452:
	if ( ++( p) == ( pe) )
		goto _test_eof1452;
case 1452:
#line 35146 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1727;
	}
	goto st1452;
tr1727:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1453;
st1453:
	if ( ++( p) == ( pe) )
		goto _test_eof1453;
case 1453:
#line 35162 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1728;
		case 32: goto tr1728;
		case 62: goto tr1729;
	}
	goto tr1658;
st1454:
	if ( ++( p) == ( pe) )
		goto _test_eof1454;
case 1454:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1730;
tr1730:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1455;
st1455:
	if ( ++( p) == ( pe) )
		goto _test_eof1455;
case 1455:
#line 35187 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1727;
	}
	goto st1455;
tr1724:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1456;
st1456:
	if ( ++( p) == ( pe) )
		goto _test_eof1456;
case 1456:
#line 35203 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1732;
		case 32: goto tr1732;
		case 62: goto tr1734;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1456;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1456;
	} else
		goto st1456;
	goto tr1658;
st1457:
	if ( ++( p) == ( pe) )
		goto _test_eof1457;
case 1457:
	switch( (*( p)) ) {
		case 66: goto st1458;
		case 68: goto st1472;
		case 72: goto st1483;
		case 82: goto st1507;
		case 98: goto st1458;
		case 100: goto st1472;
		case 104: goto st1483;
		case 114: goto st1507;
	}
	goto tr1658;
st1458:
	if ( ++( p) == ( pe) )
		goto _test_eof1458;
case 1458:
	switch( (*( p)) ) {
		case 79: goto st1459;
		case 111: goto st1459;
	}
	goto tr1658;
st1459:
	if ( ++( p) == ( pe) )
		goto _test_eof1459;
case 1459:
	switch( (*( p)) ) {
		case 68: goto st1460;
		case 100: goto st1460;
	}
	goto tr1658;
st1460:
	if ( ++( p) == ( pe) )
		goto _test_eof1460;
case 1460:
	switch( (*( p)) ) {
		case 89: goto st1461;
		case 121: goto st1461;
	}
	goto tr1658;
st1461:
	if ( ++( p) == ( pe) )
		goto _test_eof1461;
case 1461:
	switch( (*( p)) ) {
		case 9: goto st1462;
		case 32: goto st1462;
		case 62: goto tr1743;
	}
	goto tr1658;
tr1756:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1462;
tr1760:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1462;
st1462:
	if ( ++( p) == ( pe) )
		goto _test_eof1462;
case 1462:
#line 35284 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1462;
		case 32: goto st1462;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1744;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1744;
	} else
		goto tr1744;
	goto tr1658;
tr1744:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1463;
st1463:
	if ( ++( p) == ( pe) )
		goto _test_eof1463;
case 1463:
#line 35306 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1745;
		case 32: goto tr1745;
		case 61: goto tr1747;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1463;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1463;
	} else
		goto st1463;
	goto tr1658;
tr1745:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1464;
st1464:
	if ( ++( p) == ( pe) )
		goto _test_eof1464;
case 1464:
#line 35329 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1464;
		case 32: goto st1464;
		case 61: goto st1465;
	}
	goto tr1658;
tr1747:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1465;
st1465:
	if ( ++( p) == ( pe) )
		goto _test_eof1465;
case 1465:
#line 35344 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1465;
		case 32: goto st1465;
		case 34: goto st1466;
		case 39: goto st1469;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1752;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1752;
	} else
		goto tr1752;
	goto tr1658;
st1466:
	if ( ++( p) == ( pe) )
		goto _test_eof1466;
case 1466:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1753;
tr1753:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1467;
st1467:
	if ( ++( p) == ( pe) )
		goto _test_eof1467;
case 1467:
#line 35378 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1755;
	}
	goto st1467;
tr1755:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1468;
st1468:
	if ( ++( p) == ( pe) )
		goto _test_eof1468;
case 1468:
#line 35394 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1756;
		case 32: goto tr1756;
		case 62: goto tr1757;
	}
	goto tr1658;
st1469:
	if ( ++( p) == ( pe) )
		goto _test_eof1469;
case 1469:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1758;
tr1758:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1470;
st1470:
	if ( ++( p) == ( pe) )
		goto _test_eof1470;
case 1470:
#line 35419 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1755;
	}
	goto st1470;
tr1752:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1471;
st1471:
	if ( ++( p) == ( pe) )
		goto _test_eof1471;
case 1471:
#line 35435 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1760;
		case 32: goto tr1760;
		case 62: goto tr1762;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1471;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1471;
	} else
		goto st1471;
	goto tr1658;
st1472:
	if ( ++( p) == ( pe) )
		goto _test_eof1472;
case 1472:
	switch( (*( p)) ) {
		case 9: goto st1473;
		case 32: goto st1473;
		case 62: goto tr1764;
	}
	goto tr1658;
tr1777:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1473;
tr1781:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1473;
st1473:
	if ( ++( p) == ( pe) )
		goto _test_eof1473;
case 1473:
#line 35474 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1473;
		case 32: goto st1473;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1765;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1765;
	} else
		goto tr1765;
	goto tr1658;
tr1765:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1474;
st1474:
	if ( ++( p) == ( pe) )
		goto _test_eof1474;
case 1474:
#line 35496 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1766;
		case 32: goto tr1766;
		case 61: goto tr1768;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1474;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1474;
	} else
		goto st1474;
	goto tr1658;
tr1766:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1475;
st1475:
	if ( ++( p) == ( pe) )
		goto _test_eof1475;
case 1475:
#line 35519 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1475;
		case 32: goto st1475;
		case 61: goto st1476;
	}
	goto tr1658;
tr1768:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1476;
st1476:
	if ( ++( p) == ( pe) )
		goto _test_eof1476;
case 1476:
#line 35534 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1476;
		case 32: goto st1476;
		case 34: goto st1477;
		case 39: goto st1480;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1773;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1773;
	} else
		goto tr1773;
	goto tr1658;
st1477:
	if ( ++( p) == ( pe) )
		goto _test_eof1477;
case 1477:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1774;
tr1774:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1478;
st1478:
	if ( ++( p) == ( pe) )
		goto _test_eof1478;
case 1478:
#line 35568 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1776;
	}
	goto st1478;
tr1776:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1479;
st1479:
	if ( ++( p) == ( pe) )
		goto _test_eof1479;
case 1479:
#line 35584 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1777;
		case 32: goto tr1777;
		case 62: goto tr1778;
	}
	goto tr1658;
st1480:
	if ( ++( p) == ( pe) )
		goto _test_eof1480;
case 1480:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1779;
tr1779:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1481;
st1481:
	if ( ++( p) == ( pe) )
		goto _test_eof1481;
case 1481:
#line 35609 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1776;
	}
	goto st1481;
tr1773:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1482;
st1482:
	if ( ++( p) == ( pe) )
		goto _test_eof1482;
case 1482:
#line 35625 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1781;
		case 32: goto tr1781;
		case 62: goto tr1783;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1482;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1482;
	} else
		goto st1482;
	goto tr1658;
st1483:
	if ( ++( p) == ( pe) )
		goto _test_eof1483;
case 1483:
	switch( (*( p)) ) {
		case 9: goto st1484;
		case 32: goto st1484;
		case 62: goto tr1785;
		case 69: goto st1494;
		case 101: goto st1494;
	}
	goto tr1658;
tr1799:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1484;
tr1803:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1484;
st1484:
	if ( ++( p) == ( pe) )
		goto _test_eof1484;
case 1484:
#line 35666 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1484;
		case 32: goto st1484;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1787;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1787;
	} else
		goto tr1787;
	goto tr1658;
tr1787:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1485;
st1485:
	if ( ++( p) == ( pe) )
		goto _test_eof1485;
case 1485:
#line 35688 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1788;
		case 32: goto tr1788;
		case 61: goto tr1790;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1485;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1485;
	} else
		goto st1485;
	goto tr1658;
tr1788:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1486;
st1486:
	if ( ++( p) == ( pe) )
		goto _test_eof1486;
case 1486:
#line 35711 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1486;
		case 32: goto st1486;
		case 61: goto st1487;
	}
	goto tr1658;
tr1790:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1487;
st1487:
	if ( ++( p) == ( pe) )
		goto _test_eof1487;
case 1487:
#line 35726 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1487;
		case 32: goto st1487;
		case 34: goto st1488;
		case 39: goto st1491;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1795;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1795;
	} else
		goto tr1795;
	goto tr1658;
st1488:
	if ( ++( p) == ( pe) )
		goto _test_eof1488;
case 1488:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1796;
tr1796:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1489;
st1489:
	if ( ++( p) == ( pe) )
		goto _test_eof1489;
case 1489:
#line 35760 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1798;
	}
	goto st1489;
tr1798:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1490;
st1490:
	if ( ++( p) == ( pe) )
		goto _test_eof1490;
case 1490:
#line 35776 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1799;
		case 32: goto tr1799;
		case 62: goto tr1800;
	}
	goto tr1658;
st1491:
	if ( ++( p) == ( pe) )
		goto _test_eof1491;
case 1491:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1801;
tr1801:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1492;
st1492:
	if ( ++( p) == ( pe) )
		goto _test_eof1492;
case 1492:
#line 35801 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1798;
	}
	goto st1492;
tr1795:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1493;
st1493:
	if ( ++( p) == ( pe) )
		goto _test_eof1493;
case 1493:
#line 35817 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1803;
		case 32: goto tr1803;
		case 62: goto tr1805;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1493;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1493;
	} else
		goto st1493;
	goto tr1658;
st1494:
	if ( ++( p) == ( pe) )
		goto _test_eof1494;
case 1494:
	switch( (*( p)) ) {
		case 65: goto st1495;
		case 97: goto st1495;
	}
	goto tr1658;
st1495:
	if ( ++( p) == ( pe) )
		goto _test_eof1495;
case 1495:
	switch( (*( p)) ) {
		case 68: goto st1496;
		case 100: goto st1496;
	}
	goto tr1658;
st1496:
	if ( ++( p) == ( pe) )
		goto _test_eof1496;
case 1496:
	switch( (*( p)) ) {
		case 9: goto st1497;
		case 32: goto st1497;
		case 62: goto tr1809;
	}
	goto tr1658;
tr1822:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1497;
tr1826:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1497;
st1497:
	if ( ++( p) == ( pe) )
		goto _test_eof1497;
case 1497:
#line 35874 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1497;
		case 32: goto st1497;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1810;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1810;
	} else
		goto tr1810;
	goto tr1658;
tr1810:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1498;
st1498:
	if ( ++( p) == ( pe) )
		goto _test_eof1498;
case 1498:
#line 35896 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1811;
		case 32: goto tr1811;
		case 61: goto tr1813;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1498;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1498;
	} else
		goto st1498;
	goto tr1658;
tr1811:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1499;
st1499:
	if ( ++( p) == ( pe) )
		goto _test_eof1499;
case 1499:
#line 35919 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1499;
		case 32: goto st1499;
		case 61: goto st1500;
	}
	goto tr1658;
tr1813:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1500;
st1500:
	if ( ++( p) == ( pe) )
		goto _test_eof1500;
case 1500:
#line 35934 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1500;
		case 32: goto st1500;
		case 34: goto st1501;
		case 39: goto st1504;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1818;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1818;
	} else
		goto tr1818;
	goto tr1658;
st1501:
	if ( ++( p) == ( pe) )
		goto _test_eof1501;
case 1501:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1819;
tr1819:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1502;
st1502:
	if ( ++( p) == ( pe) )
		goto _test_eof1502;
case 1502:
#line 35968 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1821;
	}
	goto st1502;
tr1821:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1503;
st1503:
	if ( ++( p) == ( pe) )
		goto _test_eof1503;
case 1503:
#line 35984 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1822;
		case 32: goto tr1822;
		case 62: goto tr1823;
	}
	goto tr1658;
st1504:
	if ( ++( p) == ( pe) )
		goto _test_eof1504;
case 1504:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1824;
tr1824:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1505;
st1505:
	if ( ++( p) == ( pe) )
		goto _test_eof1505;
case 1505:
#line 36009 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1821;
	}
	goto st1505;
tr1818:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1506;
st1506:
	if ( ++( p) == ( pe) )
		goto _test_eof1506;
case 1506:
#line 36025 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1826;
		case 32: goto tr1826;
		case 62: goto tr1828;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1506;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1506;
	} else
		goto st1506;
	goto tr1658;
st1507:
	if ( ++( p) == ( pe) )
		goto _test_eof1507;
case 1507:
	switch( (*( p)) ) {
		case 9: goto st1508;
		case 32: goto st1508;
		case 62: goto tr1830;
	}
	goto tr1658;
tr1843:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1508;
tr1847:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1508;
st1508:
	if ( ++( p) == ( pe) )
		goto _test_eof1508;
case 1508:
#line 36064 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1508;
		case 32: goto st1508;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1831;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1831;
	} else
		goto tr1831;
	goto tr1658;
tr1831:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1509;
st1509:
	if ( ++( p) == ( pe) )
		goto _test_eof1509;
case 1509:
#line 36086 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1832;
		case 32: goto tr1832;
		case 61: goto tr1834;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1509;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1509;
	} else
		goto st1509;
	goto tr1658;
tr1832:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1510;
st1510:
	if ( ++( p) == ( pe) )
		goto _test_eof1510;
case 1510:
#line 36109 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1510;
		case 32: goto st1510;
		case 61: goto st1511;
	}
	goto tr1658;
tr1834:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1511;
st1511:
	if ( ++( p) == ( pe) )
		goto _test_eof1511;
case 1511:
#line 36124 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1511;
		case 32: goto st1511;
		case 34: goto st1512;
		case 39: goto st1515;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1839;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1839;
	} else
		goto tr1839;
	goto tr1658;
st1512:
	if ( ++( p) == ( pe) )
		goto _test_eof1512;
case 1512:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1840;
tr1840:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1513;
st1513:
	if ( ++( p) == ( pe) )
		goto _test_eof1513;
case 1513:
#line 36158 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1842;
	}
	goto st1513;
tr1842:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1514;
st1514:
	if ( ++( p) == ( pe) )
		goto _test_eof1514;
case 1514:
#line 36174 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1843;
		case 32: goto tr1843;
		case 62: goto tr1844;
	}
	goto tr1658;
st1515:
	if ( ++( p) == ( pe) )
		goto _test_eof1515;
case 1515:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1845;
tr1845:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1516;
st1516:
	if ( ++( p) == ( pe) )
		goto _test_eof1516;
case 1516:
#line 36199 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1842;
	}
	goto st1516;
tr1839:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1517;
st1517:
	if ( ++( p) == ( pe) )
		goto _test_eof1517;
case 1517:
#line 36215 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1847;
		case 32: goto tr1847;
		case 62: goto tr1849;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1517;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1517;
	} else
		goto st1517;
	goto tr1658;
tr2421:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1895;
st1895:
	if ( ++( p) == ( pe) )
		goto _test_eof1895;
case 1895:
#line 36238 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1518;
		case 67: goto st1541;
		case 84: goto st1569;
		case 99: goto st1541;
		case 116: goto st1569;
	}
	goto tr2422;
st1518:
	if ( ++( p) == ( pe) )
		goto _test_eof1518;
case 1518:
	switch( (*( p)) ) {
		case 67: goto st1519;
		case 84: goto st1527;
		case 99: goto st1519;
		case 116: goto st1527;
	}
	goto tr1658;
st1519:
	if ( ++( p) == ( pe) )
		goto _test_eof1519;
case 1519:
	switch( (*( p)) ) {
		case 79: goto st1520;
		case 111: goto st1520;
	}
	goto tr1658;
st1520:
	if ( ++( p) == ( pe) )
		goto _test_eof1520;
case 1520:
	switch( (*( p)) ) {
		case 76: goto st1521;
		case 108: goto st1521;
	}
	goto tr1658;
st1521:
	if ( ++( p) == ( pe) )
		goto _test_eof1521;
case 1521:
	switch( (*( p)) ) {
		case 71: goto st1522;
		case 103: goto st1522;
	}
	goto tr1658;
st1522:
	if ( ++( p) == ( pe) )
		goto _test_eof1522;
case 1522:
	switch( (*( p)) ) {
		case 82: goto st1523;
		case 114: goto st1523;
	}
	goto tr1658;
st1523:
	if ( ++( p) == ( pe) )
		goto _test_eof1523;
case 1523:
	switch( (*( p)) ) {
		case 79: goto st1524;
		case 111: goto st1524;
	}
	goto tr1658;
st1524:
	if ( ++( p) == ( pe) )
		goto _test_eof1524;
case 1524:
	switch( (*( p)) ) {
		case 85: goto st1525;
		case 117: goto st1525;
	}
	goto tr1658;
st1525:
	if ( ++( p) == ( pe) )
		goto _test_eof1525;
case 1525:
	switch( (*( p)) ) {
		case 80: goto st1526;
		case 112: goto st1526;
	}
	goto tr1658;
st1526:
	if ( ++( p) == ( pe) )
		goto _test_eof1526;
case 1526:
	if ( (*( p)) == 93 )
		goto tr1668;
	goto tr1658;
st1527:
	if ( ++( p) == ( pe) )
		goto _test_eof1527;
case 1527:
	switch( (*( p)) ) {
		case 65: goto st1528;
		case 66: goto st1532;
		case 72: goto st1536;
		case 82: goto st1540;
		case 97: goto st1528;
		case 98: goto st1532;
		case 104: goto st1536;
		case 114: goto st1540;
	}
	goto tr1658;
st1528:
	if ( ++( p) == ( pe) )
		goto _test_eof1528;
case 1528:
	switch( (*( p)) ) {
		case 66: goto st1529;
		case 98: goto st1529;
	}
	goto tr1658;
st1529:
	if ( ++( p) == ( pe) )
		goto _test_eof1529;
case 1529:
	switch( (*( p)) ) {
		case 76: goto st1530;
		case 108: goto st1530;
	}
	goto tr1658;
st1530:
	if ( ++( p) == ( pe) )
		goto _test_eof1530;
case 1530:
	switch( (*( p)) ) {
		case 69: goto st1531;
		case 101: goto st1531;
	}
	goto tr1658;
st1531:
	if ( ++( p) == ( pe) )
		goto _test_eof1531;
case 1531:
	if ( (*( p)) == 93 )
		goto tr1676;
	goto tr1658;
st1532:
	if ( ++( p) == ( pe) )
		goto _test_eof1532;
case 1532:
	switch( (*( p)) ) {
		case 79: goto st1533;
		case 111: goto st1533;
	}
	goto tr1658;
st1533:
	if ( ++( p) == ( pe) )
		goto _test_eof1533;
case 1533:
	switch( (*( p)) ) {
		case 68: goto st1534;
		case 100: goto st1534;
	}
	goto tr1658;
st1534:
	if ( ++( p) == ( pe) )
		goto _test_eof1534;
case 1534:
	switch( (*( p)) ) {
		case 89: goto st1535;
		case 121: goto st1535;
	}
	goto tr1658;
st1535:
	if ( ++( p) == ( pe) )
		goto _test_eof1535;
case 1535:
	if ( (*( p)) == 93 )
		goto tr1680;
	goto tr1658;
st1536:
	if ( ++( p) == ( pe) )
		goto _test_eof1536;
case 1536:
	switch( (*( p)) ) {
		case 69: goto st1537;
		case 101: goto st1537;
	}
	goto tr1658;
st1537:
	if ( ++( p) == ( pe) )
		goto _test_eof1537;
case 1537:
	switch( (*( p)) ) {
		case 65: goto st1538;
		case 97: goto st1538;
	}
	goto tr1658;
st1538:
	if ( ++( p) == ( pe) )
		goto _test_eof1538;
case 1538:
	switch( (*( p)) ) {
		case 68: goto st1539;
		case 100: goto st1539;
	}
	goto tr1658;
st1539:
	if ( ++( p) == ( pe) )
		goto _test_eof1539;
case 1539:
	if ( (*( p)) == 93 )
		goto tr1684;
	goto tr1658;
st1540:
	if ( ++( p) == ( pe) )
		goto _test_eof1540;
case 1540:
	if ( (*( p)) == 93 )
		goto tr1685;
	goto tr1658;
st1541:
	if ( ++( p) == ( pe) )
		goto _test_eof1541;
case 1541:
	switch( (*( p)) ) {
		case 79: goto st1542;
		case 111: goto st1542;
	}
	goto tr1658;
st1542:
	if ( ++( p) == ( pe) )
		goto _test_eof1542;
case 1542:
	switch( (*( p)) ) {
		case 76: goto st1543;
		case 108: goto st1543;
	}
	goto tr1658;
st1543:
	if ( ++( p) == ( pe) )
		goto _test_eof1543;
case 1543:
	switch( (*( p)) ) {
		case 9: goto st1544;
		case 32: goto st1544;
		case 71: goto st1554;
		case 93: goto tr1689;
		case 103: goto st1554;
	}
	goto tr1658;
tr1888:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1544;
tr1891:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1544;
st1544:
	if ( ++( p) == ( pe) )
		goto _test_eof1544;
case 1544:
#line 36496 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1544;
		case 32: goto st1544;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1876;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1876;
	} else
		goto tr1876;
	goto tr1658;
tr1876:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1545;
st1545:
	if ( ++( p) == ( pe) )
		goto _test_eof1545;
case 1545:
#line 36518 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1877;
		case 32: goto tr1877;
		case 61: goto tr1879;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1545;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1545;
	} else
		goto st1545;
	goto tr1658;
tr1877:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1546;
st1546:
	if ( ++( p) == ( pe) )
		goto _test_eof1546;
case 1546:
#line 36541 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1546;
		case 32: goto st1546;
		case 61: goto st1547;
	}
	goto tr1658;
tr1879:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1547;
st1547:
	if ( ++( p) == ( pe) )
		goto _test_eof1547;
case 1547:
#line 36556 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1547;
		case 32: goto st1547;
		case 34: goto st1548;
		case 39: goto st1551;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1884;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1884;
	} else
		goto tr1884;
	goto tr1658;
st1548:
	if ( ++( p) == ( pe) )
		goto _test_eof1548;
case 1548:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1885;
tr1885:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1549;
st1549:
	if ( ++( p) == ( pe) )
		goto _test_eof1549;
case 1549:
#line 36590 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1887;
	}
	goto st1549;
tr1887:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1550;
st1550:
	if ( ++( p) == ( pe) )
		goto _test_eof1550;
case 1550:
#line 36606 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1888;
		case 32: goto tr1888;
		case 93: goto tr1704;
	}
	goto tr1658;
st1551:
	if ( ++( p) == ( pe) )
		goto _test_eof1551;
case 1551:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1889;
tr1889:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1552;
st1552:
	if ( ++( p) == ( pe) )
		goto _test_eof1552;
case 1552:
#line 36631 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1887;
	}
	goto st1552;
tr1884:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1553;
st1553:
	if ( ++( p) == ( pe) )
		goto _test_eof1553;
case 1553:
#line 36647 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1891;
		case 32: goto tr1891;
		case 93: goto tr1709;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1553;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1553;
	} else
		goto st1553;
	goto tr1658;
st1554:
	if ( ++( p) == ( pe) )
		goto _test_eof1554;
case 1554:
	switch( (*( p)) ) {
		case 82: goto st1555;
		case 114: goto st1555;
	}
	goto tr1658;
st1555:
	if ( ++( p) == ( pe) )
		goto _test_eof1555;
case 1555:
	switch( (*( p)) ) {
		case 79: goto st1556;
		case 111: goto st1556;
	}
	goto tr1658;
st1556:
	if ( ++( p) == ( pe) )
		goto _test_eof1556;
case 1556:
	switch( (*( p)) ) {
		case 85: goto st1557;
		case 117: goto st1557;
	}
	goto tr1658;
st1557:
	if ( ++( p) == ( pe) )
		goto _test_eof1557;
case 1557:
	switch( (*( p)) ) {
		case 80: goto st1558;
		case 112: goto st1558;
	}
	goto tr1658;
st1558:
	if ( ++( p) == ( pe) )
		goto _test_eof1558;
case 1558:
	switch( (*( p)) ) {
		case 9: goto st1559;
		case 32: goto st1559;
		case 93: goto tr1715;
	}
	goto tr1658;
tr1910:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1559;
tr1913:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1559;
st1559:
	if ( ++( p) == ( pe) )
		goto _test_eof1559;
case 1559:
#line 36722 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1559;
		case 32: goto st1559;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1898;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1898;
	} else
		goto tr1898;
	goto tr1658;
tr1898:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1560;
st1560:
	if ( ++( p) == ( pe) )
		goto _test_eof1560;
case 1560:
#line 36744 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1899;
		case 32: goto tr1899;
		case 61: goto tr1901;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1560;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1560;
	} else
		goto st1560;
	goto tr1658;
tr1899:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1561;
st1561:
	if ( ++( p) == ( pe) )
		goto _test_eof1561;
case 1561:
#line 36767 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1561;
		case 32: goto st1561;
		case 61: goto st1562;
	}
	goto tr1658;
tr1901:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1562;
st1562:
	if ( ++( p) == ( pe) )
		goto _test_eof1562;
case 1562:
#line 36782 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1562;
		case 32: goto st1562;
		case 34: goto st1563;
		case 39: goto st1566;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1906;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1906;
	} else
		goto tr1906;
	goto tr1658;
st1563:
	if ( ++( p) == ( pe) )
		goto _test_eof1563;
case 1563:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1907;
tr1907:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1564;
st1564:
	if ( ++( p) == ( pe) )
		goto _test_eof1564;
case 1564:
#line 36816 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1909;
	}
	goto st1564;
tr1909:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1565;
st1565:
	if ( ++( p) == ( pe) )
		goto _test_eof1565;
case 1565:
#line 36832 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1910;
		case 32: goto tr1910;
		case 93: goto tr1729;
	}
	goto tr1658;
st1566:
	if ( ++( p) == ( pe) )
		goto _test_eof1566;
case 1566:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1911;
tr1911:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1567;
st1567:
	if ( ++( p) == ( pe) )
		goto _test_eof1567;
case 1567:
#line 36857 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1909;
	}
	goto st1567;
tr1906:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1568;
st1568:
	if ( ++( p) == ( pe) )
		goto _test_eof1568;
case 1568:
#line 36873 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1913;
		case 32: goto tr1913;
		case 93: goto tr1734;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1568;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1568;
	} else
		goto st1568;
	goto tr1658;
st1569:
	if ( ++( p) == ( pe) )
		goto _test_eof1569;
case 1569:
	switch( (*( p)) ) {
		case 66: goto st1570;
		case 68: goto st1584;
		case 72: goto st1595;
		case 82: goto st1619;
		case 98: goto st1570;
		case 100: goto st1584;
		case 104: goto st1595;
		case 114: goto st1619;
	}
	goto tr1658;
st1570:
	if ( ++( p) == ( pe) )
		goto _test_eof1570;
case 1570:
	switch( (*( p)) ) {
		case 79: goto st1571;
		case 111: goto st1571;
	}
	goto tr1658;
st1571:
	if ( ++( p) == ( pe) )
		goto _test_eof1571;
case 1571:
	switch( (*( p)) ) {
		case 68: goto st1572;
		case 100: goto st1572;
	}
	goto tr1658;
st1572:
	if ( ++( p) == ( pe) )
		goto _test_eof1572;
case 1572:
	switch( (*( p)) ) {
		case 89: goto st1573;
		case 121: goto st1573;
	}
	goto tr1658;
st1573:
	if ( ++( p) == ( pe) )
		goto _test_eof1573;
case 1573:
	switch( (*( p)) ) {
		case 9: goto st1574;
		case 32: goto st1574;
		case 93: goto tr1743;
	}
	goto tr1658;
tr1935:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1574;
tr1938:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1574;
st1574:
	if ( ++( p) == ( pe) )
		goto _test_eof1574;
case 1574:
#line 36954 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1574;
		case 32: goto st1574;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1923;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1923;
	} else
		goto tr1923;
	goto tr1658;
tr1923:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1575;
st1575:
	if ( ++( p) == ( pe) )
		goto _test_eof1575;
case 1575:
#line 36976 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1924;
		case 32: goto tr1924;
		case 61: goto tr1926;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1575;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1575;
	} else
		goto st1575;
	goto tr1658;
tr1924:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1576;
st1576:
	if ( ++( p) == ( pe) )
		goto _test_eof1576;
case 1576:
#line 36999 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1576;
		case 32: goto st1576;
		case 61: goto st1577;
	}
	goto tr1658;
tr1926:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1577;
st1577:
	if ( ++( p) == ( pe) )
		goto _test_eof1577;
case 1577:
#line 37014 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1577;
		case 32: goto st1577;
		case 34: goto st1578;
		case 39: goto st1581;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1931;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1931;
	} else
		goto tr1931;
	goto tr1658;
st1578:
	if ( ++( p) == ( pe) )
		goto _test_eof1578;
case 1578:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1932;
tr1932:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1579;
st1579:
	if ( ++( p) == ( pe) )
		goto _test_eof1579;
case 1579:
#line 37048 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1934;
	}
	goto st1579;
tr1934:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1580;
st1580:
	if ( ++( p) == ( pe) )
		goto _test_eof1580;
case 1580:
#line 37064 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1935;
		case 32: goto tr1935;
		case 93: goto tr1757;
	}
	goto tr1658;
st1581:
	if ( ++( p) == ( pe) )
		goto _test_eof1581;
case 1581:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1936;
tr1936:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1582;
st1582:
	if ( ++( p) == ( pe) )
		goto _test_eof1582;
case 1582:
#line 37089 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1934;
	}
	goto st1582;
tr1931:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1583;
st1583:
	if ( ++( p) == ( pe) )
		goto _test_eof1583;
case 1583:
#line 37105 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1938;
		case 32: goto tr1938;
		case 93: goto tr1762;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1583;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1583;
	} else
		goto st1583;
	goto tr1658;
st1584:
	if ( ++( p) == ( pe) )
		goto _test_eof1584;
case 1584:
	switch( (*( p)) ) {
		case 9: goto st1585;
		case 32: goto st1585;
		case 93: goto tr1764;
	}
	goto tr1658;
tr1953:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1585;
tr1956:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1585;
st1585:
	if ( ++( p) == ( pe) )
		goto _test_eof1585;
case 1585:
#line 37144 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1585;
		case 32: goto st1585;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1941;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1941;
	} else
		goto tr1941;
	goto tr1658;
tr1941:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1586;
st1586:
	if ( ++( p) == ( pe) )
		goto _test_eof1586;
case 1586:
#line 37166 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1942;
		case 32: goto tr1942;
		case 61: goto tr1944;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1586;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1586;
	} else
		goto st1586;
	goto tr1658;
tr1942:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1587;
st1587:
	if ( ++( p) == ( pe) )
		goto _test_eof1587;
case 1587:
#line 37189 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1587;
		case 32: goto st1587;
		case 61: goto st1588;
	}
	goto tr1658;
tr1944:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1588;
st1588:
	if ( ++( p) == ( pe) )
		goto _test_eof1588;
case 1588:
#line 37204 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1588;
		case 32: goto st1588;
		case 34: goto st1589;
		case 39: goto st1592;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1949;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1949;
	} else
		goto tr1949;
	goto tr1658;
st1589:
	if ( ++( p) == ( pe) )
		goto _test_eof1589;
case 1589:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1950;
tr1950:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1590;
st1590:
	if ( ++( p) == ( pe) )
		goto _test_eof1590;
case 1590:
#line 37238 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1952;
	}
	goto st1590;
tr1952:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1591;
st1591:
	if ( ++( p) == ( pe) )
		goto _test_eof1591;
case 1591:
#line 37254 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1953;
		case 32: goto tr1953;
		case 93: goto tr1778;
	}
	goto tr1658;
st1592:
	if ( ++( p) == ( pe) )
		goto _test_eof1592;
case 1592:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1954;
tr1954:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1593;
st1593:
	if ( ++( p) == ( pe) )
		goto _test_eof1593;
case 1593:
#line 37279 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1952;
	}
	goto st1593;
tr1949:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1594;
st1594:
	if ( ++( p) == ( pe) )
		goto _test_eof1594;
case 1594:
#line 37295 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1956;
		case 32: goto tr1956;
		case 93: goto tr1783;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1594;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1594;
	} else
		goto st1594;
	goto tr1658;
st1595:
	if ( ++( p) == ( pe) )
		goto _test_eof1595;
case 1595:
	switch( (*( p)) ) {
		case 9: goto st1596;
		case 32: goto st1596;
		case 69: goto st1606;
		case 93: goto tr1785;
		case 101: goto st1606;
	}
	goto tr1658;
tr1972:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1596;
tr1975:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1596;
st1596:
	if ( ++( p) == ( pe) )
		goto _test_eof1596;
case 1596:
#line 37336 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1596;
		case 32: goto st1596;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1960;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1960;
	} else
		goto tr1960;
	goto tr1658;
tr1960:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1597;
st1597:
	if ( ++( p) == ( pe) )
		goto _test_eof1597;
case 1597:
#line 37358 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1961;
		case 32: goto tr1961;
		case 61: goto tr1963;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1597;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1597;
	} else
		goto st1597;
	goto tr1658;
tr1961:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1598;
st1598:
	if ( ++( p) == ( pe) )
		goto _test_eof1598;
case 1598:
#line 37381 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1598;
		case 32: goto st1598;
		case 61: goto st1599;
	}
	goto tr1658;
tr1963:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1599;
st1599:
	if ( ++( p) == ( pe) )
		goto _test_eof1599;
case 1599:
#line 37396 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1599;
		case 32: goto st1599;
		case 34: goto st1600;
		case 39: goto st1603;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1968;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1968;
	} else
		goto tr1968;
	goto tr1658;
st1600:
	if ( ++( p) == ( pe) )
		goto _test_eof1600;
case 1600:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1969;
tr1969:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1601;
st1601:
	if ( ++( p) == ( pe) )
		goto _test_eof1601;
case 1601:
#line 37430 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1971;
	}
	goto st1601;
tr1971:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1602;
st1602:
	if ( ++( p) == ( pe) )
		goto _test_eof1602;
case 1602:
#line 37446 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1972;
		case 32: goto tr1972;
		case 93: goto tr1800;
	}
	goto tr1658;
st1603:
	if ( ++( p) == ( pe) )
		goto _test_eof1603;
case 1603:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1973;
tr1973:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1604;
st1604:
	if ( ++( p) == ( pe) )
		goto _test_eof1604;
case 1604:
#line 37471 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1971;
	}
	goto st1604;
tr1968:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1605;
st1605:
	if ( ++( p) == ( pe) )
		goto _test_eof1605;
case 1605:
#line 37487 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1975;
		case 32: goto tr1975;
		case 93: goto tr1805;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1605;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1605;
	} else
		goto st1605;
	goto tr1658;
st1606:
	if ( ++( p) == ( pe) )
		goto _test_eof1606;
case 1606:
	switch( (*( p)) ) {
		case 65: goto st1607;
		case 97: goto st1607;
	}
	goto tr1658;
st1607:
	if ( ++( p) == ( pe) )
		goto _test_eof1607;
case 1607:
	switch( (*( p)) ) {
		case 68: goto st1608;
		case 100: goto st1608;
	}
	goto tr1658;
st1608:
	if ( ++( p) == ( pe) )
		goto _test_eof1608;
case 1608:
	switch( (*( p)) ) {
		case 9: goto st1609;
		case 32: goto st1609;
		case 93: goto tr1809;
	}
	goto tr1658;
tr1992:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1609;
tr1995:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1609;
st1609:
	if ( ++( p) == ( pe) )
		goto _test_eof1609;
case 1609:
#line 37544 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1609;
		case 32: goto st1609;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1980;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1980;
	} else
		goto tr1980;
	goto tr1658;
tr1980:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1610;
st1610:
	if ( ++( p) == ( pe) )
		goto _test_eof1610;
case 1610:
#line 37566 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1981;
		case 32: goto tr1981;
		case 61: goto tr1983;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1610;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1610;
	} else
		goto st1610;
	goto tr1658;
tr1981:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1611;
st1611:
	if ( ++( p) == ( pe) )
		goto _test_eof1611;
case 1611:
#line 37589 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1611;
		case 32: goto st1611;
		case 61: goto st1612;
	}
	goto tr1658;
tr1983:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1612;
st1612:
	if ( ++( p) == ( pe) )
		goto _test_eof1612;
case 1612:
#line 37604 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1612;
		case 32: goto st1612;
		case 34: goto st1613;
		case 39: goto st1616;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1988;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1988;
	} else
		goto tr1988;
	goto tr1658;
st1613:
	if ( ++( p) == ( pe) )
		goto _test_eof1613;
case 1613:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1989;
tr1989:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1614;
st1614:
	if ( ++( p) == ( pe) )
		goto _test_eof1614;
case 1614:
#line 37638 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr1991;
	}
	goto st1614;
tr1991:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1615;
st1615:
	if ( ++( p) == ( pe) )
		goto _test_eof1615;
case 1615:
#line 37654 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1992;
		case 32: goto tr1992;
		case 93: goto tr1823;
	}
	goto tr1658;
st1616:
	if ( ++( p) == ( pe) )
		goto _test_eof1616;
case 1616:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr1993;
tr1993:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1617;
st1617:
	if ( ++( p) == ( pe) )
		goto _test_eof1617;
case 1617:
#line 37679 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr1991;
	}
	goto st1617;
tr1988:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1618;
st1618:
	if ( ++( p) == ( pe) )
		goto _test_eof1618;
case 1618:
#line 37695 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1995;
		case 32: goto tr1995;
		case 93: goto tr1828;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1618;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1618;
	} else
		goto st1618;
	goto tr1658;
st1619:
	if ( ++( p) == ( pe) )
		goto _test_eof1619;
case 1619:
	switch( (*( p)) ) {
		case 9: goto st1620;
		case 32: goto st1620;
		case 93: goto tr1830;
	}
	goto tr1658;
tr2010:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1620;
tr2013:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1620;
st1620:
	if ( ++( p) == ( pe) )
		goto _test_eof1620;
case 1620:
#line 37734 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1620;
		case 32: goto st1620;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1998;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1998;
	} else
		goto tr1998;
	goto tr1658;
tr1998:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1621;
st1621:
	if ( ++( p) == ( pe) )
		goto _test_eof1621;
case 1621:
#line 37756 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1999;
		case 32: goto tr1999;
		case 61: goto tr2001;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1621;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1621;
	} else
		goto st1621;
	goto tr1658;
tr1999:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1622;
st1622:
	if ( ++( p) == ( pe) )
		goto _test_eof1622;
case 1622:
#line 37779 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1622;
		case 32: goto st1622;
		case 61: goto st1623;
	}
	goto tr1658;
tr2001:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1623;
st1623:
	if ( ++( p) == ( pe) )
		goto _test_eof1623;
case 1623:
#line 37794 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1623;
		case 32: goto st1623;
		case 34: goto st1624;
		case 39: goto st1627;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2006;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2006;
	} else
		goto tr2006;
	goto tr1658;
st1624:
	if ( ++( p) == ( pe) )
		goto _test_eof1624;
case 1624:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr2007;
tr2007:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1625;
st1625:
	if ( ++( p) == ( pe) )
		goto _test_eof1625;
case 1625:
#line 37828 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 34: goto tr2009;
	}
	goto st1625;
tr2009:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1626;
st1626:
	if ( ++( p) == ( pe) )
		goto _test_eof1626;
case 1626:
#line 37844 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2010;
		case 32: goto tr2010;
		case 93: goto tr1844;
	}
	goto tr1658;
st1627:
	if ( ++( p) == ( pe) )
		goto _test_eof1627;
case 1627:
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
	}
	goto tr2011;
tr2011:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1628;
st1628:
	if ( ++( p) == ( pe) )
		goto _test_eof1628;
case 1628:
#line 37869 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1658;
		case 10: goto tr1658;
		case 13: goto tr1658;
		case 39: goto tr2009;
	}
	goto st1628;
tr2006:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1629;
st1629:
	if ( ++( p) == ( pe) )
		goto _test_eof1629;
case 1629:
#line 37885 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2013;
		case 32: goto tr2013;
		case 93: goto tr1849;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1629;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1629;
	} else
		goto st1629;
	goto tr1658;
	}
	_test_eof1630: ( cs) = 1630; goto _test_eof; 
	_test_eof1631: ( cs) = 1631; goto _test_eof; 
	_test_eof1: ( cs) = 1; goto _test_eof; 
	_test_eof1632: ( cs) = 1632; goto _test_eof; 
	_test_eof2: ( cs) = 2; goto _test_eof; 
	_test_eof1633: ( cs) = 1633; goto _test_eof; 
	_test_eof3: ( cs) = 3; goto _test_eof; 
	_test_eof4: ( cs) = 4; goto _test_eof; 
	_test_eof5: ( cs) = 5; goto _test_eof; 
	_test_eof6: ( cs) = 6; goto _test_eof; 
	_test_eof7: ( cs) = 7; goto _test_eof; 
	_test_eof8: ( cs) = 8; goto _test_eof; 
	_test_eof9: ( cs) = 9; goto _test_eof; 
	_test_eof10: ( cs) = 10; goto _test_eof; 
	_test_eof11: ( cs) = 11; goto _test_eof; 
	_test_eof12: ( cs) = 12; goto _test_eof; 
	_test_eof13: ( cs) = 13; goto _test_eof; 
	_test_eof14: ( cs) = 14; goto _test_eof; 
	_test_eof15: ( cs) = 15; goto _test_eof; 
	_test_eof16: ( cs) = 16; goto _test_eof; 
	_test_eof1634: ( cs) = 1634; goto _test_eof; 
	_test_eof17: ( cs) = 17; goto _test_eof; 
	_test_eof18: ( cs) = 18; goto _test_eof; 
	_test_eof19: ( cs) = 19; goto _test_eof; 
	_test_eof20: ( cs) = 20; goto _test_eof; 
	_test_eof21: ( cs) = 21; goto _test_eof; 
	_test_eof22: ( cs) = 22; goto _test_eof; 
	_test_eof1635: ( cs) = 1635; goto _test_eof; 
	_test_eof23: ( cs) = 23; goto _test_eof; 
	_test_eof24: ( cs) = 24; goto _test_eof; 
	_test_eof25: ( cs) = 25; goto _test_eof; 
	_test_eof26: ( cs) = 26; goto _test_eof; 
	_test_eof27: ( cs) = 27; goto _test_eof; 
	_test_eof28: ( cs) = 28; goto _test_eof; 
	_test_eof29: ( cs) = 29; goto _test_eof; 
	_test_eof30: ( cs) = 30; goto _test_eof; 
	_test_eof31: ( cs) = 31; goto _test_eof; 
	_test_eof32: ( cs) = 32; goto _test_eof; 
	_test_eof33: ( cs) = 33; goto _test_eof; 
	_test_eof34: ( cs) = 34; goto _test_eof; 
	_test_eof35: ( cs) = 35; goto _test_eof; 
	_test_eof36: ( cs) = 36; goto _test_eof; 
	_test_eof37: ( cs) = 37; goto _test_eof; 
	_test_eof38: ( cs) = 38; goto _test_eof; 
	_test_eof39: ( cs) = 39; goto _test_eof; 
	_test_eof40: ( cs) = 40; goto _test_eof; 
	_test_eof41: ( cs) = 41; goto _test_eof; 
	_test_eof42: ( cs) = 42; goto _test_eof; 
	_test_eof43: ( cs) = 43; goto _test_eof; 
	_test_eof44: ( cs) = 44; goto _test_eof; 
	_test_eof1636: ( cs) = 1636; goto _test_eof; 
	_test_eof45: ( cs) = 45; goto _test_eof; 
	_test_eof46: ( cs) = 46; goto _test_eof; 
	_test_eof47: ( cs) = 47; goto _test_eof; 
	_test_eof48: ( cs) = 48; goto _test_eof; 
	_test_eof49: ( cs) = 49; goto _test_eof; 
	_test_eof50: ( cs) = 50; goto _test_eof; 
	_test_eof51: ( cs) = 51; goto _test_eof; 
	_test_eof52: ( cs) = 52; goto _test_eof; 
	_test_eof53: ( cs) = 53; goto _test_eof; 
	_test_eof54: ( cs) = 54; goto _test_eof; 
	_test_eof55: ( cs) = 55; goto _test_eof; 
	_test_eof56: ( cs) = 56; goto _test_eof; 
	_test_eof57: ( cs) = 57; goto _test_eof; 
	_test_eof58: ( cs) = 58; goto _test_eof; 
	_test_eof59: ( cs) = 59; goto _test_eof; 
	_test_eof1637: ( cs) = 1637; goto _test_eof; 
	_test_eof60: ( cs) = 60; goto _test_eof; 
	_test_eof61: ( cs) = 61; goto _test_eof; 
	_test_eof62: ( cs) = 62; goto _test_eof; 
	_test_eof63: ( cs) = 63; goto _test_eof; 
	_test_eof64: ( cs) = 64; goto _test_eof; 
	_test_eof65: ( cs) = 65; goto _test_eof; 
	_test_eof66: ( cs) = 66; goto _test_eof; 
	_test_eof67: ( cs) = 67; goto _test_eof; 
	_test_eof68: ( cs) = 68; goto _test_eof; 
	_test_eof69: ( cs) = 69; goto _test_eof; 
	_test_eof70: ( cs) = 70; goto _test_eof; 
	_test_eof71: ( cs) = 71; goto _test_eof; 
	_test_eof72: ( cs) = 72; goto _test_eof; 
	_test_eof73: ( cs) = 73; goto _test_eof; 
	_test_eof74: ( cs) = 74; goto _test_eof; 
	_test_eof1638: ( cs) = 1638; goto _test_eof; 
	_test_eof1639: ( cs) = 1639; goto _test_eof; 
	_test_eof75: ( cs) = 75; goto _test_eof; 
	_test_eof1640: ( cs) = 1640; goto _test_eof; 
	_test_eof1641: ( cs) = 1641; goto _test_eof; 
	_test_eof76: ( cs) = 76; goto _test_eof; 
	_test_eof1642: ( cs) = 1642; goto _test_eof; 
	_test_eof77: ( cs) = 77; goto _test_eof; 
	_test_eof78: ( cs) = 78; goto _test_eof; 
	_test_eof79: ( cs) = 79; goto _test_eof; 
	_test_eof1643: ( cs) = 1643; goto _test_eof; 
	_test_eof1644: ( cs) = 1644; goto _test_eof; 
	_test_eof80: ( cs) = 80; goto _test_eof; 
	_test_eof81: ( cs) = 81; goto _test_eof; 
	_test_eof82: ( cs) = 82; goto _test_eof; 
	_test_eof83: ( cs) = 83; goto _test_eof; 
	_test_eof84: ( cs) = 84; goto _test_eof; 
	_test_eof85: ( cs) = 85; goto _test_eof; 
	_test_eof86: ( cs) = 86; goto _test_eof; 
	_test_eof87: ( cs) = 87; goto _test_eof; 
	_test_eof88: ( cs) = 88; goto _test_eof; 
	_test_eof89: ( cs) = 89; goto _test_eof; 
	_test_eof1645: ( cs) = 1645; goto _test_eof; 
	_test_eof90: ( cs) = 90; goto _test_eof; 
	_test_eof91: ( cs) = 91; goto _test_eof; 
	_test_eof92: ( cs) = 92; goto _test_eof; 
	_test_eof93: ( cs) = 93; goto _test_eof; 
	_test_eof94: ( cs) = 94; goto _test_eof; 
	_test_eof95: ( cs) = 95; goto _test_eof; 
	_test_eof96: ( cs) = 96; goto _test_eof; 
	_test_eof97: ( cs) = 97; goto _test_eof; 
	_test_eof98: ( cs) = 98; goto _test_eof; 
	_test_eof99: ( cs) = 99; goto _test_eof; 
	_test_eof1646: ( cs) = 1646; goto _test_eof; 
	_test_eof100: ( cs) = 100; goto _test_eof; 
	_test_eof101: ( cs) = 101; goto _test_eof; 
	_test_eof102: ( cs) = 102; goto _test_eof; 
	_test_eof103: ( cs) = 103; goto _test_eof; 
	_test_eof104: ( cs) = 104; goto _test_eof; 
	_test_eof105: ( cs) = 105; goto _test_eof; 
	_test_eof106: ( cs) = 106; goto _test_eof; 
	_test_eof1647: ( cs) = 1647; goto _test_eof; 
	_test_eof107: ( cs) = 107; goto _test_eof; 
	_test_eof1648: ( cs) = 1648; goto _test_eof; 
	_test_eof108: ( cs) = 108; goto _test_eof; 
	_test_eof109: ( cs) = 109; goto _test_eof; 
	_test_eof110: ( cs) = 110; goto _test_eof; 
	_test_eof111: ( cs) = 111; goto _test_eof; 
	_test_eof112: ( cs) = 112; goto _test_eof; 
	_test_eof113: ( cs) = 113; goto _test_eof; 
	_test_eof114: ( cs) = 114; goto _test_eof; 
	_test_eof115: ( cs) = 115; goto _test_eof; 
	_test_eof116: ( cs) = 116; goto _test_eof; 
	_test_eof1649: ( cs) = 1649; goto _test_eof; 
	_test_eof117: ( cs) = 117; goto _test_eof; 
	_test_eof1650: ( cs) = 1650; goto _test_eof; 
	_test_eof118: ( cs) = 118; goto _test_eof; 
	_test_eof119: ( cs) = 119; goto _test_eof; 
	_test_eof120: ( cs) = 120; goto _test_eof; 
	_test_eof121: ( cs) = 121; goto _test_eof; 
	_test_eof122: ( cs) = 122; goto _test_eof; 
	_test_eof123: ( cs) = 123; goto _test_eof; 
	_test_eof124: ( cs) = 124; goto _test_eof; 
	_test_eof1651: ( cs) = 1651; goto _test_eof; 
	_test_eof125: ( cs) = 125; goto _test_eof; 
	_test_eof126: ( cs) = 126; goto _test_eof; 
	_test_eof127: ( cs) = 127; goto _test_eof; 
	_test_eof128: ( cs) = 128; goto _test_eof; 
	_test_eof129: ( cs) = 129; goto _test_eof; 
	_test_eof130: ( cs) = 130; goto _test_eof; 
	_test_eof131: ( cs) = 131; goto _test_eof; 
	_test_eof132: ( cs) = 132; goto _test_eof; 
	_test_eof1652: ( cs) = 1652; goto _test_eof; 
	_test_eof133: ( cs) = 133; goto _test_eof; 
	_test_eof134: ( cs) = 134; goto _test_eof; 
	_test_eof135: ( cs) = 135; goto _test_eof; 
	_test_eof1653: ( cs) = 1653; goto _test_eof; 
	_test_eof136: ( cs) = 136; goto _test_eof; 
	_test_eof137: ( cs) = 137; goto _test_eof; 
	_test_eof138: ( cs) = 138; goto _test_eof; 
	_test_eof139: ( cs) = 139; goto _test_eof; 
	_test_eof140: ( cs) = 140; goto _test_eof; 
	_test_eof141: ( cs) = 141; goto _test_eof; 
	_test_eof142: ( cs) = 142; goto _test_eof; 
	_test_eof143: ( cs) = 143; goto _test_eof; 
	_test_eof144: ( cs) = 144; goto _test_eof; 
	_test_eof145: ( cs) = 145; goto _test_eof; 
	_test_eof146: ( cs) = 146; goto _test_eof; 
	_test_eof147: ( cs) = 147; goto _test_eof; 
	_test_eof148: ( cs) = 148; goto _test_eof; 
	_test_eof149: ( cs) = 149; goto _test_eof; 
	_test_eof150: ( cs) = 150; goto _test_eof; 
	_test_eof151: ( cs) = 151; goto _test_eof; 
	_test_eof152: ( cs) = 152; goto _test_eof; 
	_test_eof153: ( cs) = 153; goto _test_eof; 
	_test_eof154: ( cs) = 154; goto _test_eof; 
	_test_eof155: ( cs) = 155; goto _test_eof; 
	_test_eof156: ( cs) = 156; goto _test_eof; 
	_test_eof157: ( cs) = 157; goto _test_eof; 
	_test_eof158: ( cs) = 158; goto _test_eof; 
	_test_eof159: ( cs) = 159; goto _test_eof; 
	_test_eof160: ( cs) = 160; goto _test_eof; 
	_test_eof161: ( cs) = 161; goto _test_eof; 
	_test_eof162: ( cs) = 162; goto _test_eof; 
	_test_eof163: ( cs) = 163; goto _test_eof; 
	_test_eof164: ( cs) = 164; goto _test_eof; 
	_test_eof165: ( cs) = 165; goto _test_eof; 
	_test_eof166: ( cs) = 166; goto _test_eof; 
	_test_eof167: ( cs) = 167; goto _test_eof; 
	_test_eof168: ( cs) = 168; goto _test_eof; 
	_test_eof169: ( cs) = 169; goto _test_eof; 
	_test_eof170: ( cs) = 170; goto _test_eof; 
	_test_eof171: ( cs) = 171; goto _test_eof; 
	_test_eof172: ( cs) = 172; goto _test_eof; 
	_test_eof173: ( cs) = 173; goto _test_eof; 
	_test_eof1654: ( cs) = 1654; goto _test_eof; 
	_test_eof1655: ( cs) = 1655; goto _test_eof; 
	_test_eof1656: ( cs) = 1656; goto _test_eof; 
	_test_eof1657: ( cs) = 1657; goto _test_eof; 
	_test_eof174: ( cs) = 174; goto _test_eof; 
	_test_eof175: ( cs) = 175; goto _test_eof; 
	_test_eof176: ( cs) = 176; goto _test_eof; 
	_test_eof177: ( cs) = 177; goto _test_eof; 
	_test_eof178: ( cs) = 178; goto _test_eof; 
	_test_eof179: ( cs) = 179; goto _test_eof; 
	_test_eof180: ( cs) = 180; goto _test_eof; 
	_test_eof181: ( cs) = 181; goto _test_eof; 
	_test_eof182: ( cs) = 182; goto _test_eof; 
	_test_eof183: ( cs) = 183; goto _test_eof; 
	_test_eof184: ( cs) = 184; goto _test_eof; 
	_test_eof185: ( cs) = 185; goto _test_eof; 
	_test_eof186: ( cs) = 186; goto _test_eof; 
	_test_eof187: ( cs) = 187; goto _test_eof; 
	_test_eof188: ( cs) = 188; goto _test_eof; 
	_test_eof189: ( cs) = 189; goto _test_eof; 
	_test_eof190: ( cs) = 190; goto _test_eof; 
	_test_eof191: ( cs) = 191; goto _test_eof; 
	_test_eof192: ( cs) = 192; goto _test_eof; 
	_test_eof1658: ( cs) = 1658; goto _test_eof; 
	_test_eof193: ( cs) = 193; goto _test_eof; 
	_test_eof194: ( cs) = 194; goto _test_eof; 
	_test_eof195: ( cs) = 195; goto _test_eof; 
	_test_eof196: ( cs) = 196; goto _test_eof; 
	_test_eof197: ( cs) = 197; goto _test_eof; 
	_test_eof198: ( cs) = 198; goto _test_eof; 
	_test_eof199: ( cs) = 199; goto _test_eof; 
	_test_eof200: ( cs) = 200; goto _test_eof; 
	_test_eof201: ( cs) = 201; goto _test_eof; 
	_test_eof1659: ( cs) = 1659; goto _test_eof; 
	_test_eof1660: ( cs) = 1660; goto _test_eof; 
	_test_eof1661: ( cs) = 1661; goto _test_eof; 
	_test_eof202: ( cs) = 202; goto _test_eof; 
	_test_eof203: ( cs) = 203; goto _test_eof; 
	_test_eof204: ( cs) = 204; goto _test_eof; 
	_test_eof1662: ( cs) = 1662; goto _test_eof; 
	_test_eof1663: ( cs) = 1663; goto _test_eof; 
	_test_eof1664: ( cs) = 1664; goto _test_eof; 
	_test_eof205: ( cs) = 205; goto _test_eof; 
	_test_eof1665: ( cs) = 1665; goto _test_eof; 
	_test_eof206: ( cs) = 206; goto _test_eof; 
	_test_eof1666: ( cs) = 1666; goto _test_eof; 
	_test_eof207: ( cs) = 207; goto _test_eof; 
	_test_eof208: ( cs) = 208; goto _test_eof; 
	_test_eof209: ( cs) = 209; goto _test_eof; 
	_test_eof210: ( cs) = 210; goto _test_eof; 
	_test_eof211: ( cs) = 211; goto _test_eof; 
	_test_eof212: ( cs) = 212; goto _test_eof; 
	_test_eof213: ( cs) = 213; goto _test_eof; 
	_test_eof214: ( cs) = 214; goto _test_eof; 
	_test_eof215: ( cs) = 215; goto _test_eof; 
	_test_eof216: ( cs) = 216; goto _test_eof; 
	_test_eof217: ( cs) = 217; goto _test_eof; 
	_test_eof218: ( cs) = 218; goto _test_eof; 
	_test_eof219: ( cs) = 219; goto _test_eof; 
	_test_eof1667: ( cs) = 1667; goto _test_eof; 
	_test_eof220: ( cs) = 220; goto _test_eof; 
	_test_eof221: ( cs) = 221; goto _test_eof; 
	_test_eof222: ( cs) = 222; goto _test_eof; 
	_test_eof223: ( cs) = 223; goto _test_eof; 
	_test_eof224: ( cs) = 224; goto _test_eof; 
	_test_eof225: ( cs) = 225; goto _test_eof; 
	_test_eof1668: ( cs) = 1668; goto _test_eof; 
	_test_eof226: ( cs) = 226; goto _test_eof; 
	_test_eof227: ( cs) = 227; goto _test_eof; 
	_test_eof228: ( cs) = 228; goto _test_eof; 
	_test_eof229: ( cs) = 229; goto _test_eof; 
	_test_eof230: ( cs) = 230; goto _test_eof; 
	_test_eof231: ( cs) = 231; goto _test_eof; 
	_test_eof232: ( cs) = 232; goto _test_eof; 
	_test_eof233: ( cs) = 233; goto _test_eof; 
	_test_eof234: ( cs) = 234; goto _test_eof; 
	_test_eof235: ( cs) = 235; goto _test_eof; 
	_test_eof236: ( cs) = 236; goto _test_eof; 
	_test_eof237: ( cs) = 237; goto _test_eof; 
	_test_eof238: ( cs) = 238; goto _test_eof; 
	_test_eof239: ( cs) = 239; goto _test_eof; 
	_test_eof240: ( cs) = 240; goto _test_eof; 
	_test_eof241: ( cs) = 241; goto _test_eof; 
	_test_eof242: ( cs) = 242; goto _test_eof; 
	_test_eof243: ( cs) = 243; goto _test_eof; 
	_test_eof244: ( cs) = 244; goto _test_eof; 
	_test_eof245: ( cs) = 245; goto _test_eof; 
	_test_eof246: ( cs) = 246; goto _test_eof; 
	_test_eof1669: ( cs) = 1669; goto _test_eof; 
	_test_eof247: ( cs) = 247; goto _test_eof; 
	_test_eof248: ( cs) = 248; goto _test_eof; 
	_test_eof249: ( cs) = 249; goto _test_eof; 
	_test_eof250: ( cs) = 250; goto _test_eof; 
	_test_eof251: ( cs) = 251; goto _test_eof; 
	_test_eof252: ( cs) = 252; goto _test_eof; 
	_test_eof253: ( cs) = 253; goto _test_eof; 
	_test_eof254: ( cs) = 254; goto _test_eof; 
	_test_eof255: ( cs) = 255; goto _test_eof; 
	_test_eof256: ( cs) = 256; goto _test_eof; 
	_test_eof257: ( cs) = 257; goto _test_eof; 
	_test_eof258: ( cs) = 258; goto _test_eof; 
	_test_eof259: ( cs) = 259; goto _test_eof; 
	_test_eof260: ( cs) = 260; goto _test_eof; 
	_test_eof261: ( cs) = 261; goto _test_eof; 
	_test_eof262: ( cs) = 262; goto _test_eof; 
	_test_eof263: ( cs) = 263; goto _test_eof; 
	_test_eof264: ( cs) = 264; goto _test_eof; 
	_test_eof265: ( cs) = 265; goto _test_eof; 
	_test_eof266: ( cs) = 266; goto _test_eof; 
	_test_eof267: ( cs) = 267; goto _test_eof; 
	_test_eof268: ( cs) = 268; goto _test_eof; 
	_test_eof269: ( cs) = 269; goto _test_eof; 
	_test_eof270: ( cs) = 270; goto _test_eof; 
	_test_eof271: ( cs) = 271; goto _test_eof; 
	_test_eof272: ( cs) = 272; goto _test_eof; 
	_test_eof273: ( cs) = 273; goto _test_eof; 
	_test_eof274: ( cs) = 274; goto _test_eof; 
	_test_eof275: ( cs) = 275; goto _test_eof; 
	_test_eof276: ( cs) = 276; goto _test_eof; 
	_test_eof277: ( cs) = 277; goto _test_eof; 
	_test_eof278: ( cs) = 278; goto _test_eof; 
	_test_eof279: ( cs) = 279; goto _test_eof; 
	_test_eof280: ( cs) = 280; goto _test_eof; 
	_test_eof281: ( cs) = 281; goto _test_eof; 
	_test_eof1670: ( cs) = 1670; goto _test_eof; 
	_test_eof282: ( cs) = 282; goto _test_eof; 
	_test_eof283: ( cs) = 283; goto _test_eof; 
	_test_eof284: ( cs) = 284; goto _test_eof; 
	_test_eof285: ( cs) = 285; goto _test_eof; 
	_test_eof286: ( cs) = 286; goto _test_eof; 
	_test_eof287: ( cs) = 287; goto _test_eof; 
	_test_eof288: ( cs) = 288; goto _test_eof; 
	_test_eof289: ( cs) = 289; goto _test_eof; 
	_test_eof290: ( cs) = 290; goto _test_eof; 
	_test_eof291: ( cs) = 291; goto _test_eof; 
	_test_eof1671: ( cs) = 1671; goto _test_eof; 
	_test_eof1672: ( cs) = 1672; goto _test_eof; 
	_test_eof292: ( cs) = 292; goto _test_eof; 
	_test_eof293: ( cs) = 293; goto _test_eof; 
	_test_eof294: ( cs) = 294; goto _test_eof; 
	_test_eof295: ( cs) = 295; goto _test_eof; 
	_test_eof296: ( cs) = 296; goto _test_eof; 
	_test_eof297: ( cs) = 297; goto _test_eof; 
	_test_eof298: ( cs) = 298; goto _test_eof; 
	_test_eof299: ( cs) = 299; goto _test_eof; 
	_test_eof300: ( cs) = 300; goto _test_eof; 
	_test_eof301: ( cs) = 301; goto _test_eof; 
	_test_eof302: ( cs) = 302; goto _test_eof; 
	_test_eof303: ( cs) = 303; goto _test_eof; 
	_test_eof304: ( cs) = 304; goto _test_eof; 
	_test_eof305: ( cs) = 305; goto _test_eof; 
	_test_eof306: ( cs) = 306; goto _test_eof; 
	_test_eof307: ( cs) = 307; goto _test_eof; 
	_test_eof308: ( cs) = 308; goto _test_eof; 
	_test_eof309: ( cs) = 309; goto _test_eof; 
	_test_eof310: ( cs) = 310; goto _test_eof; 
	_test_eof311: ( cs) = 311; goto _test_eof; 
	_test_eof312: ( cs) = 312; goto _test_eof; 
	_test_eof313: ( cs) = 313; goto _test_eof; 
	_test_eof314: ( cs) = 314; goto _test_eof; 
	_test_eof315: ( cs) = 315; goto _test_eof; 
	_test_eof316: ( cs) = 316; goto _test_eof; 
	_test_eof317: ( cs) = 317; goto _test_eof; 
	_test_eof318: ( cs) = 318; goto _test_eof; 
	_test_eof319: ( cs) = 319; goto _test_eof; 
	_test_eof320: ( cs) = 320; goto _test_eof; 
	_test_eof321: ( cs) = 321; goto _test_eof; 
	_test_eof322: ( cs) = 322; goto _test_eof; 
	_test_eof323: ( cs) = 323; goto _test_eof; 
	_test_eof324: ( cs) = 324; goto _test_eof; 
	_test_eof325: ( cs) = 325; goto _test_eof; 
	_test_eof326: ( cs) = 326; goto _test_eof; 
	_test_eof327: ( cs) = 327; goto _test_eof; 
	_test_eof328: ( cs) = 328; goto _test_eof; 
	_test_eof329: ( cs) = 329; goto _test_eof; 
	_test_eof330: ( cs) = 330; goto _test_eof; 
	_test_eof331: ( cs) = 331; goto _test_eof; 
	_test_eof332: ( cs) = 332; goto _test_eof; 
	_test_eof333: ( cs) = 333; goto _test_eof; 
	_test_eof1673: ( cs) = 1673; goto _test_eof; 
	_test_eof334: ( cs) = 334; goto _test_eof; 
	_test_eof335: ( cs) = 335; goto _test_eof; 
	_test_eof336: ( cs) = 336; goto _test_eof; 
	_test_eof337: ( cs) = 337; goto _test_eof; 
	_test_eof338: ( cs) = 338; goto _test_eof; 
	_test_eof339: ( cs) = 339; goto _test_eof; 
	_test_eof340: ( cs) = 340; goto _test_eof; 
	_test_eof341: ( cs) = 341; goto _test_eof; 
	_test_eof342: ( cs) = 342; goto _test_eof; 
	_test_eof343: ( cs) = 343; goto _test_eof; 
	_test_eof344: ( cs) = 344; goto _test_eof; 
	_test_eof345: ( cs) = 345; goto _test_eof; 
	_test_eof346: ( cs) = 346; goto _test_eof; 
	_test_eof347: ( cs) = 347; goto _test_eof; 
	_test_eof348: ( cs) = 348; goto _test_eof; 
	_test_eof349: ( cs) = 349; goto _test_eof; 
	_test_eof350: ( cs) = 350; goto _test_eof; 
	_test_eof351: ( cs) = 351; goto _test_eof; 
	_test_eof352: ( cs) = 352; goto _test_eof; 
	_test_eof353: ( cs) = 353; goto _test_eof; 
	_test_eof354: ( cs) = 354; goto _test_eof; 
	_test_eof355: ( cs) = 355; goto _test_eof; 
	_test_eof356: ( cs) = 356; goto _test_eof; 
	_test_eof357: ( cs) = 357; goto _test_eof; 
	_test_eof358: ( cs) = 358; goto _test_eof; 
	_test_eof359: ( cs) = 359; goto _test_eof; 
	_test_eof360: ( cs) = 360; goto _test_eof; 
	_test_eof361: ( cs) = 361; goto _test_eof; 
	_test_eof362: ( cs) = 362; goto _test_eof; 
	_test_eof363: ( cs) = 363; goto _test_eof; 
	_test_eof364: ( cs) = 364; goto _test_eof; 
	_test_eof365: ( cs) = 365; goto _test_eof; 
	_test_eof366: ( cs) = 366; goto _test_eof; 
	_test_eof367: ( cs) = 367; goto _test_eof; 
	_test_eof368: ( cs) = 368; goto _test_eof; 
	_test_eof369: ( cs) = 369; goto _test_eof; 
	_test_eof370: ( cs) = 370; goto _test_eof; 
	_test_eof371: ( cs) = 371; goto _test_eof; 
	_test_eof372: ( cs) = 372; goto _test_eof; 
	_test_eof373: ( cs) = 373; goto _test_eof; 
	_test_eof374: ( cs) = 374; goto _test_eof; 
	_test_eof375: ( cs) = 375; goto _test_eof; 
	_test_eof376: ( cs) = 376; goto _test_eof; 
	_test_eof377: ( cs) = 377; goto _test_eof; 
	_test_eof378: ( cs) = 378; goto _test_eof; 
	_test_eof379: ( cs) = 379; goto _test_eof; 
	_test_eof380: ( cs) = 380; goto _test_eof; 
	_test_eof381: ( cs) = 381; goto _test_eof; 
	_test_eof382: ( cs) = 382; goto _test_eof; 
	_test_eof1674: ( cs) = 1674; goto _test_eof; 
	_test_eof383: ( cs) = 383; goto _test_eof; 
	_test_eof384: ( cs) = 384; goto _test_eof; 
	_test_eof385: ( cs) = 385; goto _test_eof; 
	_test_eof1675: ( cs) = 1675; goto _test_eof; 
	_test_eof386: ( cs) = 386; goto _test_eof; 
	_test_eof387: ( cs) = 387; goto _test_eof; 
	_test_eof388: ( cs) = 388; goto _test_eof; 
	_test_eof389: ( cs) = 389; goto _test_eof; 
	_test_eof390: ( cs) = 390; goto _test_eof; 
	_test_eof391: ( cs) = 391; goto _test_eof; 
	_test_eof392: ( cs) = 392; goto _test_eof; 
	_test_eof393: ( cs) = 393; goto _test_eof; 
	_test_eof394: ( cs) = 394; goto _test_eof; 
	_test_eof395: ( cs) = 395; goto _test_eof; 
	_test_eof396: ( cs) = 396; goto _test_eof; 
	_test_eof1676: ( cs) = 1676; goto _test_eof; 
	_test_eof397: ( cs) = 397; goto _test_eof; 
	_test_eof398: ( cs) = 398; goto _test_eof; 
	_test_eof399: ( cs) = 399; goto _test_eof; 
	_test_eof400: ( cs) = 400; goto _test_eof; 
	_test_eof401: ( cs) = 401; goto _test_eof; 
	_test_eof402: ( cs) = 402; goto _test_eof; 
	_test_eof403: ( cs) = 403; goto _test_eof; 
	_test_eof404: ( cs) = 404; goto _test_eof; 
	_test_eof405: ( cs) = 405; goto _test_eof; 
	_test_eof406: ( cs) = 406; goto _test_eof; 
	_test_eof407: ( cs) = 407; goto _test_eof; 
	_test_eof408: ( cs) = 408; goto _test_eof; 
	_test_eof409: ( cs) = 409; goto _test_eof; 
	_test_eof1677: ( cs) = 1677; goto _test_eof; 
	_test_eof410: ( cs) = 410; goto _test_eof; 
	_test_eof411: ( cs) = 411; goto _test_eof; 
	_test_eof412: ( cs) = 412; goto _test_eof; 
	_test_eof413: ( cs) = 413; goto _test_eof; 
	_test_eof414: ( cs) = 414; goto _test_eof; 
	_test_eof415: ( cs) = 415; goto _test_eof; 
	_test_eof416: ( cs) = 416; goto _test_eof; 
	_test_eof417: ( cs) = 417; goto _test_eof; 
	_test_eof418: ( cs) = 418; goto _test_eof; 
	_test_eof419: ( cs) = 419; goto _test_eof; 
	_test_eof420: ( cs) = 420; goto _test_eof; 
	_test_eof421: ( cs) = 421; goto _test_eof; 
	_test_eof422: ( cs) = 422; goto _test_eof; 
	_test_eof423: ( cs) = 423; goto _test_eof; 
	_test_eof424: ( cs) = 424; goto _test_eof; 
	_test_eof425: ( cs) = 425; goto _test_eof; 
	_test_eof426: ( cs) = 426; goto _test_eof; 
	_test_eof427: ( cs) = 427; goto _test_eof; 
	_test_eof428: ( cs) = 428; goto _test_eof; 
	_test_eof429: ( cs) = 429; goto _test_eof; 
	_test_eof430: ( cs) = 430; goto _test_eof; 
	_test_eof431: ( cs) = 431; goto _test_eof; 
	_test_eof1678: ( cs) = 1678; goto _test_eof; 
	_test_eof432: ( cs) = 432; goto _test_eof; 
	_test_eof433: ( cs) = 433; goto _test_eof; 
	_test_eof434: ( cs) = 434; goto _test_eof; 
	_test_eof435: ( cs) = 435; goto _test_eof; 
	_test_eof436: ( cs) = 436; goto _test_eof; 
	_test_eof437: ( cs) = 437; goto _test_eof; 
	_test_eof438: ( cs) = 438; goto _test_eof; 
	_test_eof439: ( cs) = 439; goto _test_eof; 
	_test_eof440: ( cs) = 440; goto _test_eof; 
	_test_eof441: ( cs) = 441; goto _test_eof; 
	_test_eof442: ( cs) = 442; goto _test_eof; 
	_test_eof1679: ( cs) = 1679; goto _test_eof; 
	_test_eof443: ( cs) = 443; goto _test_eof; 
	_test_eof444: ( cs) = 444; goto _test_eof; 
	_test_eof445: ( cs) = 445; goto _test_eof; 
	_test_eof446: ( cs) = 446; goto _test_eof; 
	_test_eof447: ( cs) = 447; goto _test_eof; 
	_test_eof448: ( cs) = 448; goto _test_eof; 
	_test_eof449: ( cs) = 449; goto _test_eof; 
	_test_eof450: ( cs) = 450; goto _test_eof; 
	_test_eof451: ( cs) = 451; goto _test_eof; 
	_test_eof452: ( cs) = 452; goto _test_eof; 
	_test_eof453: ( cs) = 453; goto _test_eof; 
	_test_eof1680: ( cs) = 1680; goto _test_eof; 
	_test_eof454: ( cs) = 454; goto _test_eof; 
	_test_eof455: ( cs) = 455; goto _test_eof; 
	_test_eof456: ( cs) = 456; goto _test_eof; 
	_test_eof457: ( cs) = 457; goto _test_eof; 
	_test_eof458: ( cs) = 458; goto _test_eof; 
	_test_eof459: ( cs) = 459; goto _test_eof; 
	_test_eof460: ( cs) = 460; goto _test_eof; 
	_test_eof461: ( cs) = 461; goto _test_eof; 
	_test_eof462: ( cs) = 462; goto _test_eof; 
	_test_eof463: ( cs) = 463; goto _test_eof; 
	_test_eof464: ( cs) = 464; goto _test_eof; 
	_test_eof465: ( cs) = 465; goto _test_eof; 
	_test_eof466: ( cs) = 466; goto _test_eof; 
	_test_eof467: ( cs) = 467; goto _test_eof; 
	_test_eof468: ( cs) = 468; goto _test_eof; 
	_test_eof469: ( cs) = 469; goto _test_eof; 
	_test_eof470: ( cs) = 470; goto _test_eof; 
	_test_eof471: ( cs) = 471; goto _test_eof; 
	_test_eof472: ( cs) = 472; goto _test_eof; 
	_test_eof473: ( cs) = 473; goto _test_eof; 
	_test_eof474: ( cs) = 474; goto _test_eof; 
	_test_eof475: ( cs) = 475; goto _test_eof; 
	_test_eof476: ( cs) = 476; goto _test_eof; 
	_test_eof477: ( cs) = 477; goto _test_eof; 
	_test_eof478: ( cs) = 478; goto _test_eof; 
	_test_eof479: ( cs) = 479; goto _test_eof; 
	_test_eof480: ( cs) = 480; goto _test_eof; 
	_test_eof481: ( cs) = 481; goto _test_eof; 
	_test_eof482: ( cs) = 482; goto _test_eof; 
	_test_eof483: ( cs) = 483; goto _test_eof; 
	_test_eof484: ( cs) = 484; goto _test_eof; 
	_test_eof485: ( cs) = 485; goto _test_eof; 
	_test_eof486: ( cs) = 486; goto _test_eof; 
	_test_eof487: ( cs) = 487; goto _test_eof; 
	_test_eof488: ( cs) = 488; goto _test_eof; 
	_test_eof489: ( cs) = 489; goto _test_eof; 
	_test_eof490: ( cs) = 490; goto _test_eof; 
	_test_eof491: ( cs) = 491; goto _test_eof; 
	_test_eof492: ( cs) = 492; goto _test_eof; 
	_test_eof493: ( cs) = 493; goto _test_eof; 
	_test_eof494: ( cs) = 494; goto _test_eof; 
	_test_eof495: ( cs) = 495; goto _test_eof; 
	_test_eof496: ( cs) = 496; goto _test_eof; 
	_test_eof497: ( cs) = 497; goto _test_eof; 
	_test_eof498: ( cs) = 498; goto _test_eof; 
	_test_eof499: ( cs) = 499; goto _test_eof; 
	_test_eof500: ( cs) = 500; goto _test_eof; 
	_test_eof1681: ( cs) = 1681; goto _test_eof; 
	_test_eof501: ( cs) = 501; goto _test_eof; 
	_test_eof502: ( cs) = 502; goto _test_eof; 
	_test_eof503: ( cs) = 503; goto _test_eof; 
	_test_eof504: ( cs) = 504; goto _test_eof; 
	_test_eof505: ( cs) = 505; goto _test_eof; 
	_test_eof506: ( cs) = 506; goto _test_eof; 
	_test_eof507: ( cs) = 507; goto _test_eof; 
	_test_eof508: ( cs) = 508; goto _test_eof; 
	_test_eof509: ( cs) = 509; goto _test_eof; 
	_test_eof1682: ( cs) = 1682; goto _test_eof; 
	_test_eof1683: ( cs) = 1683; goto _test_eof; 
	_test_eof510: ( cs) = 510; goto _test_eof; 
	_test_eof511: ( cs) = 511; goto _test_eof; 
	_test_eof512: ( cs) = 512; goto _test_eof; 
	_test_eof513: ( cs) = 513; goto _test_eof; 
	_test_eof1684: ( cs) = 1684; goto _test_eof; 
	_test_eof1685: ( cs) = 1685; goto _test_eof; 
	_test_eof514: ( cs) = 514; goto _test_eof; 
	_test_eof515: ( cs) = 515; goto _test_eof; 
	_test_eof516: ( cs) = 516; goto _test_eof; 
	_test_eof517: ( cs) = 517; goto _test_eof; 
	_test_eof518: ( cs) = 518; goto _test_eof; 
	_test_eof519: ( cs) = 519; goto _test_eof; 
	_test_eof520: ( cs) = 520; goto _test_eof; 
	_test_eof521: ( cs) = 521; goto _test_eof; 
	_test_eof522: ( cs) = 522; goto _test_eof; 
	_test_eof1686: ( cs) = 1686; goto _test_eof; 
	_test_eof1687: ( cs) = 1687; goto _test_eof; 
	_test_eof523: ( cs) = 523; goto _test_eof; 
	_test_eof524: ( cs) = 524; goto _test_eof; 
	_test_eof525: ( cs) = 525; goto _test_eof; 
	_test_eof526: ( cs) = 526; goto _test_eof; 
	_test_eof527: ( cs) = 527; goto _test_eof; 
	_test_eof528: ( cs) = 528; goto _test_eof; 
	_test_eof529: ( cs) = 529; goto _test_eof; 
	_test_eof530: ( cs) = 530; goto _test_eof; 
	_test_eof531: ( cs) = 531; goto _test_eof; 
	_test_eof532: ( cs) = 532; goto _test_eof; 
	_test_eof533: ( cs) = 533; goto _test_eof; 
	_test_eof534: ( cs) = 534; goto _test_eof; 
	_test_eof535: ( cs) = 535; goto _test_eof; 
	_test_eof536: ( cs) = 536; goto _test_eof; 
	_test_eof537: ( cs) = 537; goto _test_eof; 
	_test_eof538: ( cs) = 538; goto _test_eof; 
	_test_eof539: ( cs) = 539; goto _test_eof; 
	_test_eof540: ( cs) = 540; goto _test_eof; 
	_test_eof541: ( cs) = 541; goto _test_eof; 
	_test_eof542: ( cs) = 542; goto _test_eof; 
	_test_eof543: ( cs) = 543; goto _test_eof; 
	_test_eof544: ( cs) = 544; goto _test_eof; 
	_test_eof545: ( cs) = 545; goto _test_eof; 
	_test_eof546: ( cs) = 546; goto _test_eof; 
	_test_eof547: ( cs) = 547; goto _test_eof; 
	_test_eof548: ( cs) = 548; goto _test_eof; 
	_test_eof549: ( cs) = 549; goto _test_eof; 
	_test_eof550: ( cs) = 550; goto _test_eof; 
	_test_eof551: ( cs) = 551; goto _test_eof; 
	_test_eof552: ( cs) = 552; goto _test_eof; 
	_test_eof553: ( cs) = 553; goto _test_eof; 
	_test_eof554: ( cs) = 554; goto _test_eof; 
	_test_eof555: ( cs) = 555; goto _test_eof; 
	_test_eof556: ( cs) = 556; goto _test_eof; 
	_test_eof557: ( cs) = 557; goto _test_eof; 
	_test_eof558: ( cs) = 558; goto _test_eof; 
	_test_eof559: ( cs) = 559; goto _test_eof; 
	_test_eof560: ( cs) = 560; goto _test_eof; 
	_test_eof561: ( cs) = 561; goto _test_eof; 
	_test_eof562: ( cs) = 562; goto _test_eof; 
	_test_eof563: ( cs) = 563; goto _test_eof; 
	_test_eof564: ( cs) = 564; goto _test_eof; 
	_test_eof565: ( cs) = 565; goto _test_eof; 
	_test_eof1688: ( cs) = 1688; goto _test_eof; 
	_test_eof1689: ( cs) = 1689; goto _test_eof; 
	_test_eof566: ( cs) = 566; goto _test_eof; 
	_test_eof1690: ( cs) = 1690; goto _test_eof; 
	_test_eof1691: ( cs) = 1691; goto _test_eof; 
	_test_eof567: ( cs) = 567; goto _test_eof; 
	_test_eof568: ( cs) = 568; goto _test_eof; 
	_test_eof569: ( cs) = 569; goto _test_eof; 
	_test_eof570: ( cs) = 570; goto _test_eof; 
	_test_eof571: ( cs) = 571; goto _test_eof; 
	_test_eof572: ( cs) = 572; goto _test_eof; 
	_test_eof573: ( cs) = 573; goto _test_eof; 
	_test_eof574: ( cs) = 574; goto _test_eof; 
	_test_eof1692: ( cs) = 1692; goto _test_eof; 
	_test_eof1693: ( cs) = 1693; goto _test_eof; 
	_test_eof575: ( cs) = 575; goto _test_eof; 
	_test_eof1694: ( cs) = 1694; goto _test_eof; 
	_test_eof576: ( cs) = 576; goto _test_eof; 
	_test_eof577: ( cs) = 577; goto _test_eof; 
	_test_eof578: ( cs) = 578; goto _test_eof; 
	_test_eof579: ( cs) = 579; goto _test_eof; 
	_test_eof580: ( cs) = 580; goto _test_eof; 
	_test_eof581: ( cs) = 581; goto _test_eof; 
	_test_eof582: ( cs) = 582; goto _test_eof; 
	_test_eof583: ( cs) = 583; goto _test_eof; 
	_test_eof584: ( cs) = 584; goto _test_eof; 
	_test_eof585: ( cs) = 585; goto _test_eof; 
	_test_eof586: ( cs) = 586; goto _test_eof; 
	_test_eof587: ( cs) = 587; goto _test_eof; 
	_test_eof588: ( cs) = 588; goto _test_eof; 
	_test_eof589: ( cs) = 589; goto _test_eof; 
	_test_eof590: ( cs) = 590; goto _test_eof; 
	_test_eof591: ( cs) = 591; goto _test_eof; 
	_test_eof592: ( cs) = 592; goto _test_eof; 
	_test_eof593: ( cs) = 593; goto _test_eof; 
	_test_eof1695: ( cs) = 1695; goto _test_eof; 
	_test_eof594: ( cs) = 594; goto _test_eof; 
	_test_eof595: ( cs) = 595; goto _test_eof; 
	_test_eof596: ( cs) = 596; goto _test_eof; 
	_test_eof597: ( cs) = 597; goto _test_eof; 
	_test_eof598: ( cs) = 598; goto _test_eof; 
	_test_eof599: ( cs) = 599; goto _test_eof; 
	_test_eof600: ( cs) = 600; goto _test_eof; 
	_test_eof601: ( cs) = 601; goto _test_eof; 
	_test_eof602: ( cs) = 602; goto _test_eof; 
	_test_eof1696: ( cs) = 1696; goto _test_eof; 
	_test_eof1697: ( cs) = 1697; goto _test_eof; 
	_test_eof1698: ( cs) = 1698; goto _test_eof; 
	_test_eof1699: ( cs) = 1699; goto _test_eof; 
	_test_eof1700: ( cs) = 1700; goto _test_eof; 
	_test_eof603: ( cs) = 603; goto _test_eof; 
	_test_eof604: ( cs) = 604; goto _test_eof; 
	_test_eof1701: ( cs) = 1701; goto _test_eof; 
	_test_eof1702: ( cs) = 1702; goto _test_eof; 
	_test_eof1703: ( cs) = 1703; goto _test_eof; 
	_test_eof1704: ( cs) = 1704; goto _test_eof; 
	_test_eof1705: ( cs) = 1705; goto _test_eof; 
	_test_eof1706: ( cs) = 1706; goto _test_eof; 
	_test_eof605: ( cs) = 605; goto _test_eof; 
	_test_eof606: ( cs) = 606; goto _test_eof; 
	_test_eof1707: ( cs) = 1707; goto _test_eof; 
	_test_eof607: ( cs) = 607; goto _test_eof; 
	_test_eof608: ( cs) = 608; goto _test_eof; 
	_test_eof609: ( cs) = 609; goto _test_eof; 
	_test_eof610: ( cs) = 610; goto _test_eof; 
	_test_eof611: ( cs) = 611; goto _test_eof; 
	_test_eof612: ( cs) = 612; goto _test_eof; 
	_test_eof613: ( cs) = 613; goto _test_eof; 
	_test_eof614: ( cs) = 614; goto _test_eof; 
	_test_eof615: ( cs) = 615; goto _test_eof; 
	_test_eof1708: ( cs) = 1708; goto _test_eof; 
	_test_eof1709: ( cs) = 1709; goto _test_eof; 
	_test_eof1710: ( cs) = 1710; goto _test_eof; 
	_test_eof1711: ( cs) = 1711; goto _test_eof; 
	_test_eof1712: ( cs) = 1712; goto _test_eof; 
	_test_eof616: ( cs) = 616; goto _test_eof; 
	_test_eof617: ( cs) = 617; goto _test_eof; 
	_test_eof618: ( cs) = 618; goto _test_eof; 
	_test_eof619: ( cs) = 619; goto _test_eof; 
	_test_eof620: ( cs) = 620; goto _test_eof; 
	_test_eof621: ( cs) = 621; goto _test_eof; 
	_test_eof622: ( cs) = 622; goto _test_eof; 
	_test_eof623: ( cs) = 623; goto _test_eof; 
	_test_eof624: ( cs) = 624; goto _test_eof; 
	_test_eof625: ( cs) = 625; goto _test_eof; 
	_test_eof1713: ( cs) = 1713; goto _test_eof; 
	_test_eof1714: ( cs) = 1714; goto _test_eof; 
	_test_eof1715: ( cs) = 1715; goto _test_eof; 
	_test_eof1716: ( cs) = 1716; goto _test_eof; 
	_test_eof626: ( cs) = 626; goto _test_eof; 
	_test_eof627: ( cs) = 627; goto _test_eof; 
	_test_eof1717: ( cs) = 1717; goto _test_eof; 
	_test_eof1718: ( cs) = 1718; goto _test_eof; 
	_test_eof1719: ( cs) = 1719; goto _test_eof; 
	_test_eof628: ( cs) = 628; goto _test_eof; 
	_test_eof629: ( cs) = 629; goto _test_eof; 
	_test_eof1720: ( cs) = 1720; goto _test_eof; 
	_test_eof1721: ( cs) = 1721; goto _test_eof; 
	_test_eof1722: ( cs) = 1722; goto _test_eof; 
	_test_eof1723: ( cs) = 1723; goto _test_eof; 
	_test_eof1724: ( cs) = 1724; goto _test_eof; 
	_test_eof1725: ( cs) = 1725; goto _test_eof; 
	_test_eof1726: ( cs) = 1726; goto _test_eof; 
	_test_eof1727: ( cs) = 1727; goto _test_eof; 
	_test_eof630: ( cs) = 630; goto _test_eof; 
	_test_eof631: ( cs) = 631; goto _test_eof; 
	_test_eof1728: ( cs) = 1728; goto _test_eof; 
	_test_eof1729: ( cs) = 1729; goto _test_eof; 
	_test_eof1730: ( cs) = 1730; goto _test_eof; 
	_test_eof632: ( cs) = 632; goto _test_eof; 
	_test_eof633: ( cs) = 633; goto _test_eof; 
	_test_eof1731: ( cs) = 1731; goto _test_eof; 
	_test_eof1732: ( cs) = 1732; goto _test_eof; 
	_test_eof1733: ( cs) = 1733; goto _test_eof; 
	_test_eof1734: ( cs) = 1734; goto _test_eof; 
	_test_eof1735: ( cs) = 1735; goto _test_eof; 
	_test_eof1736: ( cs) = 1736; goto _test_eof; 
	_test_eof634: ( cs) = 634; goto _test_eof; 
	_test_eof635: ( cs) = 635; goto _test_eof; 
	_test_eof1737: ( cs) = 1737; goto _test_eof; 
	_test_eof636: ( cs) = 636; goto _test_eof; 
	_test_eof1738: ( cs) = 1738; goto _test_eof; 
	_test_eof1739: ( cs) = 1739; goto _test_eof; 
	_test_eof1740: ( cs) = 1740; goto _test_eof; 
	_test_eof637: ( cs) = 637; goto _test_eof; 
	_test_eof638: ( cs) = 638; goto _test_eof; 
	_test_eof1741: ( cs) = 1741; goto _test_eof; 
	_test_eof1742: ( cs) = 1742; goto _test_eof; 
	_test_eof1743: ( cs) = 1743; goto _test_eof; 
	_test_eof1744: ( cs) = 1744; goto _test_eof; 
	_test_eof1745: ( cs) = 1745; goto _test_eof; 
	_test_eof639: ( cs) = 639; goto _test_eof; 
	_test_eof640: ( cs) = 640; goto _test_eof; 
	_test_eof1746: ( cs) = 1746; goto _test_eof; 
	_test_eof1747: ( cs) = 1747; goto _test_eof; 
	_test_eof1748: ( cs) = 1748; goto _test_eof; 
	_test_eof1749: ( cs) = 1749; goto _test_eof; 
	_test_eof1750: ( cs) = 1750; goto _test_eof; 
	_test_eof641: ( cs) = 641; goto _test_eof; 
	_test_eof642: ( cs) = 642; goto _test_eof; 
	_test_eof643: ( cs) = 643; goto _test_eof; 
	_test_eof1751: ( cs) = 1751; goto _test_eof; 
	_test_eof644: ( cs) = 644; goto _test_eof; 
	_test_eof645: ( cs) = 645; goto _test_eof; 
	_test_eof646: ( cs) = 646; goto _test_eof; 
	_test_eof647: ( cs) = 647; goto _test_eof; 
	_test_eof648: ( cs) = 648; goto _test_eof; 
	_test_eof649: ( cs) = 649; goto _test_eof; 
	_test_eof650: ( cs) = 650; goto _test_eof; 
	_test_eof651: ( cs) = 651; goto _test_eof; 
	_test_eof652: ( cs) = 652; goto _test_eof; 
	_test_eof653: ( cs) = 653; goto _test_eof; 
	_test_eof654: ( cs) = 654; goto _test_eof; 
	_test_eof1752: ( cs) = 1752; goto _test_eof; 
	_test_eof655: ( cs) = 655; goto _test_eof; 
	_test_eof656: ( cs) = 656; goto _test_eof; 
	_test_eof1753: ( cs) = 1753; goto _test_eof; 
	_test_eof1754: ( cs) = 1754; goto _test_eof; 
	_test_eof1755: ( cs) = 1755; goto _test_eof; 
	_test_eof1756: ( cs) = 1756; goto _test_eof; 
	_test_eof1757: ( cs) = 1757; goto _test_eof; 
	_test_eof1758: ( cs) = 1758; goto _test_eof; 
	_test_eof1759: ( cs) = 1759; goto _test_eof; 
	_test_eof1760: ( cs) = 1760; goto _test_eof; 
	_test_eof1761: ( cs) = 1761; goto _test_eof; 
	_test_eof657: ( cs) = 657; goto _test_eof; 
	_test_eof658: ( cs) = 658; goto _test_eof; 
	_test_eof659: ( cs) = 659; goto _test_eof; 
	_test_eof660: ( cs) = 660; goto _test_eof; 
	_test_eof661: ( cs) = 661; goto _test_eof; 
	_test_eof662: ( cs) = 662; goto _test_eof; 
	_test_eof663: ( cs) = 663; goto _test_eof; 
	_test_eof664: ( cs) = 664; goto _test_eof; 
	_test_eof665: ( cs) = 665; goto _test_eof; 
	_test_eof1762: ( cs) = 1762; goto _test_eof; 
	_test_eof666: ( cs) = 666; goto _test_eof; 
	_test_eof667: ( cs) = 667; goto _test_eof; 
	_test_eof668: ( cs) = 668; goto _test_eof; 
	_test_eof669: ( cs) = 669; goto _test_eof; 
	_test_eof670: ( cs) = 670; goto _test_eof; 
	_test_eof671: ( cs) = 671; goto _test_eof; 
	_test_eof672: ( cs) = 672; goto _test_eof; 
	_test_eof673: ( cs) = 673; goto _test_eof; 
	_test_eof674: ( cs) = 674; goto _test_eof; 
	_test_eof675: ( cs) = 675; goto _test_eof; 
	_test_eof1763: ( cs) = 1763; goto _test_eof; 
	_test_eof676: ( cs) = 676; goto _test_eof; 
	_test_eof677: ( cs) = 677; goto _test_eof; 
	_test_eof678: ( cs) = 678; goto _test_eof; 
	_test_eof679: ( cs) = 679; goto _test_eof; 
	_test_eof680: ( cs) = 680; goto _test_eof; 
	_test_eof681: ( cs) = 681; goto _test_eof; 
	_test_eof682: ( cs) = 682; goto _test_eof; 
	_test_eof683: ( cs) = 683; goto _test_eof; 
	_test_eof684: ( cs) = 684; goto _test_eof; 
	_test_eof685: ( cs) = 685; goto _test_eof; 
	_test_eof686: ( cs) = 686; goto _test_eof; 
	_test_eof1764: ( cs) = 1764; goto _test_eof; 
	_test_eof687: ( cs) = 687; goto _test_eof; 
	_test_eof688: ( cs) = 688; goto _test_eof; 
	_test_eof689: ( cs) = 689; goto _test_eof; 
	_test_eof690: ( cs) = 690; goto _test_eof; 
	_test_eof691: ( cs) = 691; goto _test_eof; 
	_test_eof692: ( cs) = 692; goto _test_eof; 
	_test_eof693: ( cs) = 693; goto _test_eof; 
	_test_eof694: ( cs) = 694; goto _test_eof; 
	_test_eof695: ( cs) = 695; goto _test_eof; 
	_test_eof696: ( cs) = 696; goto _test_eof; 
	_test_eof697: ( cs) = 697; goto _test_eof; 
	_test_eof698: ( cs) = 698; goto _test_eof; 
	_test_eof699: ( cs) = 699; goto _test_eof; 
	_test_eof1765: ( cs) = 1765; goto _test_eof; 
	_test_eof700: ( cs) = 700; goto _test_eof; 
	_test_eof701: ( cs) = 701; goto _test_eof; 
	_test_eof702: ( cs) = 702; goto _test_eof; 
	_test_eof703: ( cs) = 703; goto _test_eof; 
	_test_eof704: ( cs) = 704; goto _test_eof; 
	_test_eof705: ( cs) = 705; goto _test_eof; 
	_test_eof706: ( cs) = 706; goto _test_eof; 
	_test_eof707: ( cs) = 707; goto _test_eof; 
	_test_eof708: ( cs) = 708; goto _test_eof; 
	_test_eof709: ( cs) = 709; goto _test_eof; 
	_test_eof1766: ( cs) = 1766; goto _test_eof; 
	_test_eof1767: ( cs) = 1767; goto _test_eof; 
	_test_eof1768: ( cs) = 1768; goto _test_eof; 
	_test_eof1769: ( cs) = 1769; goto _test_eof; 
	_test_eof1770: ( cs) = 1770; goto _test_eof; 
	_test_eof1771: ( cs) = 1771; goto _test_eof; 
	_test_eof1772: ( cs) = 1772; goto _test_eof; 
	_test_eof1773: ( cs) = 1773; goto _test_eof; 
	_test_eof1774: ( cs) = 1774; goto _test_eof; 
	_test_eof1775: ( cs) = 1775; goto _test_eof; 
	_test_eof1776: ( cs) = 1776; goto _test_eof; 
	_test_eof1777: ( cs) = 1777; goto _test_eof; 
	_test_eof1778: ( cs) = 1778; goto _test_eof; 
	_test_eof710: ( cs) = 710; goto _test_eof; 
	_test_eof711: ( cs) = 711; goto _test_eof; 
	_test_eof1779: ( cs) = 1779; goto _test_eof; 
	_test_eof1780: ( cs) = 1780; goto _test_eof; 
	_test_eof1781: ( cs) = 1781; goto _test_eof; 
	_test_eof1782: ( cs) = 1782; goto _test_eof; 
	_test_eof1783: ( cs) = 1783; goto _test_eof; 
	_test_eof712: ( cs) = 712; goto _test_eof; 
	_test_eof713: ( cs) = 713; goto _test_eof; 
	_test_eof1784: ( cs) = 1784; goto _test_eof; 
	_test_eof1785: ( cs) = 1785; goto _test_eof; 
	_test_eof1786: ( cs) = 1786; goto _test_eof; 
	_test_eof1787: ( cs) = 1787; goto _test_eof; 
	_test_eof714: ( cs) = 714; goto _test_eof; 
	_test_eof715: ( cs) = 715; goto _test_eof; 
	_test_eof716: ( cs) = 716; goto _test_eof; 
	_test_eof717: ( cs) = 717; goto _test_eof; 
	_test_eof718: ( cs) = 718; goto _test_eof; 
	_test_eof719: ( cs) = 719; goto _test_eof; 
	_test_eof720: ( cs) = 720; goto _test_eof; 
	_test_eof721: ( cs) = 721; goto _test_eof; 
	_test_eof722: ( cs) = 722; goto _test_eof; 
	_test_eof1788: ( cs) = 1788; goto _test_eof; 
	_test_eof1789: ( cs) = 1789; goto _test_eof; 
	_test_eof1790: ( cs) = 1790; goto _test_eof; 
	_test_eof1791: ( cs) = 1791; goto _test_eof; 
	_test_eof1792: ( cs) = 1792; goto _test_eof; 
	_test_eof723: ( cs) = 723; goto _test_eof; 
	_test_eof724: ( cs) = 724; goto _test_eof; 
	_test_eof1793: ( cs) = 1793; goto _test_eof; 
	_test_eof1794: ( cs) = 1794; goto _test_eof; 
	_test_eof1795: ( cs) = 1795; goto _test_eof; 
	_test_eof1796: ( cs) = 1796; goto _test_eof; 
	_test_eof1797: ( cs) = 1797; goto _test_eof; 
	_test_eof725: ( cs) = 725; goto _test_eof; 
	_test_eof726: ( cs) = 726; goto _test_eof; 
	_test_eof1798: ( cs) = 1798; goto _test_eof; 
	_test_eof1799: ( cs) = 1799; goto _test_eof; 
	_test_eof1800: ( cs) = 1800; goto _test_eof; 
	_test_eof727: ( cs) = 727; goto _test_eof; 
	_test_eof728: ( cs) = 728; goto _test_eof; 
	_test_eof1801: ( cs) = 1801; goto _test_eof; 
	_test_eof729: ( cs) = 729; goto _test_eof; 
	_test_eof730: ( cs) = 730; goto _test_eof; 
	_test_eof731: ( cs) = 731; goto _test_eof; 
	_test_eof732: ( cs) = 732; goto _test_eof; 
	_test_eof733: ( cs) = 733; goto _test_eof; 
	_test_eof734: ( cs) = 734; goto _test_eof; 
	_test_eof735: ( cs) = 735; goto _test_eof; 
	_test_eof736: ( cs) = 736; goto _test_eof; 
	_test_eof737: ( cs) = 737; goto _test_eof; 
	_test_eof1802: ( cs) = 1802; goto _test_eof; 
	_test_eof1803: ( cs) = 1803; goto _test_eof; 
	_test_eof1804: ( cs) = 1804; goto _test_eof; 
	_test_eof1805: ( cs) = 1805; goto _test_eof; 
	_test_eof738: ( cs) = 738; goto _test_eof; 
	_test_eof739: ( cs) = 739; goto _test_eof; 
	_test_eof1806: ( cs) = 1806; goto _test_eof; 
	_test_eof1807: ( cs) = 1807; goto _test_eof; 
	_test_eof1808: ( cs) = 1808; goto _test_eof; 
	_test_eof1809: ( cs) = 1809; goto _test_eof; 
	_test_eof1810: ( cs) = 1810; goto _test_eof; 
	_test_eof1811: ( cs) = 1811; goto _test_eof; 
	_test_eof1812: ( cs) = 1812; goto _test_eof; 
	_test_eof740: ( cs) = 740; goto _test_eof; 
	_test_eof741: ( cs) = 741; goto _test_eof; 
	_test_eof1813: ( cs) = 1813; goto _test_eof; 
	_test_eof1814: ( cs) = 1814; goto _test_eof; 
	_test_eof1815: ( cs) = 1815; goto _test_eof; 
	_test_eof1816: ( cs) = 1816; goto _test_eof; 
	_test_eof742: ( cs) = 742; goto _test_eof; 
	_test_eof743: ( cs) = 743; goto _test_eof; 
	_test_eof1817: ( cs) = 1817; goto _test_eof; 
	_test_eof1818: ( cs) = 1818; goto _test_eof; 
	_test_eof1819: ( cs) = 1819; goto _test_eof; 
	_test_eof1820: ( cs) = 1820; goto _test_eof; 
	_test_eof1821: ( cs) = 1821; goto _test_eof; 
	_test_eof744: ( cs) = 744; goto _test_eof; 
	_test_eof745: ( cs) = 745; goto _test_eof; 
	_test_eof746: ( cs) = 746; goto _test_eof; 
	_test_eof747: ( cs) = 747; goto _test_eof; 
	_test_eof748: ( cs) = 748; goto _test_eof; 
	_test_eof749: ( cs) = 749; goto _test_eof; 
	_test_eof750: ( cs) = 750; goto _test_eof; 
	_test_eof1822: ( cs) = 1822; goto _test_eof; 
	_test_eof751: ( cs) = 751; goto _test_eof; 
	_test_eof752: ( cs) = 752; goto _test_eof; 
	_test_eof753: ( cs) = 753; goto _test_eof; 
	_test_eof754: ( cs) = 754; goto _test_eof; 
	_test_eof755: ( cs) = 755; goto _test_eof; 
	_test_eof756: ( cs) = 756; goto _test_eof; 
	_test_eof757: ( cs) = 757; goto _test_eof; 
	_test_eof758: ( cs) = 758; goto _test_eof; 
	_test_eof1823: ( cs) = 1823; goto _test_eof; 
	_test_eof1824: ( cs) = 1824; goto _test_eof; 
	_test_eof1825: ( cs) = 1825; goto _test_eof; 
	_test_eof1826: ( cs) = 1826; goto _test_eof; 
	_test_eof1827: ( cs) = 1827; goto _test_eof; 
	_test_eof1828: ( cs) = 1828; goto _test_eof; 
	_test_eof1829: ( cs) = 1829; goto _test_eof; 
	_test_eof1830: ( cs) = 1830; goto _test_eof; 
	_test_eof759: ( cs) = 759; goto _test_eof; 
	_test_eof760: ( cs) = 760; goto _test_eof; 
	_test_eof1831: ( cs) = 1831; goto _test_eof; 
	_test_eof1832: ( cs) = 1832; goto _test_eof; 
	_test_eof1833: ( cs) = 1833; goto _test_eof; 
	_test_eof1834: ( cs) = 1834; goto _test_eof; 
	_test_eof1835: ( cs) = 1835; goto _test_eof; 
	_test_eof1836: ( cs) = 1836; goto _test_eof; 
	_test_eof761: ( cs) = 761; goto _test_eof; 
	_test_eof762: ( cs) = 762; goto _test_eof; 
	_test_eof1837: ( cs) = 1837; goto _test_eof; 
	_test_eof1838: ( cs) = 1838; goto _test_eof; 
	_test_eof1839: ( cs) = 1839; goto _test_eof; 
	_test_eof1840: ( cs) = 1840; goto _test_eof; 
	_test_eof1841: ( cs) = 1841; goto _test_eof; 
	_test_eof1842: ( cs) = 1842; goto _test_eof; 
	_test_eof1843: ( cs) = 1843; goto _test_eof; 
	_test_eof1844: ( cs) = 1844; goto _test_eof; 
	_test_eof1845: ( cs) = 1845; goto _test_eof; 
	_test_eof763: ( cs) = 763; goto _test_eof; 
	_test_eof764: ( cs) = 764; goto _test_eof; 
	_test_eof1846: ( cs) = 1846; goto _test_eof; 
	_test_eof1847: ( cs) = 1847; goto _test_eof; 
	_test_eof1848: ( cs) = 1848; goto _test_eof; 
	_test_eof1849: ( cs) = 1849; goto _test_eof; 
	_test_eof1850: ( cs) = 1850; goto _test_eof; 
	_test_eof765: ( cs) = 765; goto _test_eof; 
	_test_eof766: ( cs) = 766; goto _test_eof; 
	_test_eof767: ( cs) = 767; goto _test_eof; 
	_test_eof1851: ( cs) = 1851; goto _test_eof; 
	_test_eof768: ( cs) = 768; goto _test_eof; 
	_test_eof769: ( cs) = 769; goto _test_eof; 
	_test_eof770: ( cs) = 770; goto _test_eof; 
	_test_eof771: ( cs) = 771; goto _test_eof; 
	_test_eof772: ( cs) = 772; goto _test_eof; 
	_test_eof773: ( cs) = 773; goto _test_eof; 
	_test_eof774: ( cs) = 774; goto _test_eof; 
	_test_eof775: ( cs) = 775; goto _test_eof; 
	_test_eof776: ( cs) = 776; goto _test_eof; 
	_test_eof1852: ( cs) = 1852; goto _test_eof; 
	_test_eof777: ( cs) = 777; goto _test_eof; 
	_test_eof778: ( cs) = 778; goto _test_eof; 
	_test_eof779: ( cs) = 779; goto _test_eof; 
	_test_eof780: ( cs) = 780; goto _test_eof; 
	_test_eof781: ( cs) = 781; goto _test_eof; 
	_test_eof782: ( cs) = 782; goto _test_eof; 
	_test_eof1853: ( cs) = 1853; goto _test_eof; 
	_test_eof1854: ( cs) = 1854; goto _test_eof; 
	_test_eof1855: ( cs) = 1855; goto _test_eof; 
	_test_eof1856: ( cs) = 1856; goto _test_eof; 
	_test_eof1857: ( cs) = 1857; goto _test_eof; 
	_test_eof1858: ( cs) = 1858; goto _test_eof; 
	_test_eof1859: ( cs) = 1859; goto _test_eof; 
	_test_eof1860: ( cs) = 1860; goto _test_eof; 
	_test_eof1861: ( cs) = 1861; goto _test_eof; 
	_test_eof1862: ( cs) = 1862; goto _test_eof; 
	_test_eof1863: ( cs) = 1863; goto _test_eof; 
	_test_eof1864: ( cs) = 1864; goto _test_eof; 
	_test_eof1865: ( cs) = 1865; goto _test_eof; 
	_test_eof783: ( cs) = 783; goto _test_eof; 
	_test_eof784: ( cs) = 784; goto _test_eof; 
	_test_eof785: ( cs) = 785; goto _test_eof; 
	_test_eof786: ( cs) = 786; goto _test_eof; 
	_test_eof787: ( cs) = 787; goto _test_eof; 
	_test_eof788: ( cs) = 788; goto _test_eof; 
	_test_eof789: ( cs) = 789; goto _test_eof; 
	_test_eof790: ( cs) = 790; goto _test_eof; 
	_test_eof791: ( cs) = 791; goto _test_eof; 
	_test_eof792: ( cs) = 792; goto _test_eof; 
	_test_eof793: ( cs) = 793; goto _test_eof; 
	_test_eof794: ( cs) = 794; goto _test_eof; 
	_test_eof795: ( cs) = 795; goto _test_eof; 
	_test_eof796: ( cs) = 796; goto _test_eof; 
	_test_eof797: ( cs) = 797; goto _test_eof; 
	_test_eof798: ( cs) = 798; goto _test_eof; 
	_test_eof799: ( cs) = 799; goto _test_eof; 
	_test_eof800: ( cs) = 800; goto _test_eof; 
	_test_eof801: ( cs) = 801; goto _test_eof; 
	_test_eof802: ( cs) = 802; goto _test_eof; 
	_test_eof803: ( cs) = 803; goto _test_eof; 
	_test_eof804: ( cs) = 804; goto _test_eof; 
	_test_eof805: ( cs) = 805; goto _test_eof; 
	_test_eof806: ( cs) = 806; goto _test_eof; 
	_test_eof1866: ( cs) = 1866; goto _test_eof; 
	_test_eof807: ( cs) = 807; goto _test_eof; 
	_test_eof808: ( cs) = 808; goto _test_eof; 
	_test_eof809: ( cs) = 809; goto _test_eof; 
	_test_eof810: ( cs) = 810; goto _test_eof; 
	_test_eof811: ( cs) = 811; goto _test_eof; 
	_test_eof812: ( cs) = 812; goto _test_eof; 
	_test_eof813: ( cs) = 813; goto _test_eof; 
	_test_eof814: ( cs) = 814; goto _test_eof; 
	_test_eof815: ( cs) = 815; goto _test_eof; 
	_test_eof816: ( cs) = 816; goto _test_eof; 
	_test_eof817: ( cs) = 817; goto _test_eof; 
	_test_eof818: ( cs) = 818; goto _test_eof; 
	_test_eof819: ( cs) = 819; goto _test_eof; 
	_test_eof820: ( cs) = 820; goto _test_eof; 
	_test_eof1867: ( cs) = 1867; goto _test_eof; 
	_test_eof821: ( cs) = 821; goto _test_eof; 
	_test_eof822: ( cs) = 822; goto _test_eof; 
	_test_eof823: ( cs) = 823; goto _test_eof; 
	_test_eof824: ( cs) = 824; goto _test_eof; 
	_test_eof825: ( cs) = 825; goto _test_eof; 
	_test_eof826: ( cs) = 826; goto _test_eof; 
	_test_eof827: ( cs) = 827; goto _test_eof; 
	_test_eof828: ( cs) = 828; goto _test_eof; 
	_test_eof829: ( cs) = 829; goto _test_eof; 
	_test_eof830: ( cs) = 830; goto _test_eof; 
	_test_eof831: ( cs) = 831; goto _test_eof; 
	_test_eof832: ( cs) = 832; goto _test_eof; 
	_test_eof833: ( cs) = 833; goto _test_eof; 
	_test_eof834: ( cs) = 834; goto _test_eof; 
	_test_eof835: ( cs) = 835; goto _test_eof; 
	_test_eof836: ( cs) = 836; goto _test_eof; 
	_test_eof837: ( cs) = 837; goto _test_eof; 
	_test_eof838: ( cs) = 838; goto _test_eof; 
	_test_eof839: ( cs) = 839; goto _test_eof; 
	_test_eof840: ( cs) = 840; goto _test_eof; 
	_test_eof841: ( cs) = 841; goto _test_eof; 
	_test_eof842: ( cs) = 842; goto _test_eof; 
	_test_eof843: ( cs) = 843; goto _test_eof; 
	_test_eof844: ( cs) = 844; goto _test_eof; 
	_test_eof845: ( cs) = 845; goto _test_eof; 
	_test_eof846: ( cs) = 846; goto _test_eof; 
	_test_eof847: ( cs) = 847; goto _test_eof; 
	_test_eof848: ( cs) = 848; goto _test_eof; 
	_test_eof849: ( cs) = 849; goto _test_eof; 
	_test_eof850: ( cs) = 850; goto _test_eof; 
	_test_eof851: ( cs) = 851; goto _test_eof; 
	_test_eof852: ( cs) = 852; goto _test_eof; 
	_test_eof853: ( cs) = 853; goto _test_eof; 
	_test_eof854: ( cs) = 854; goto _test_eof; 
	_test_eof855: ( cs) = 855; goto _test_eof; 
	_test_eof856: ( cs) = 856; goto _test_eof; 
	_test_eof857: ( cs) = 857; goto _test_eof; 
	_test_eof858: ( cs) = 858; goto _test_eof; 
	_test_eof859: ( cs) = 859; goto _test_eof; 
	_test_eof860: ( cs) = 860; goto _test_eof; 
	_test_eof861: ( cs) = 861; goto _test_eof; 
	_test_eof862: ( cs) = 862; goto _test_eof; 
	_test_eof863: ( cs) = 863; goto _test_eof; 
	_test_eof864: ( cs) = 864; goto _test_eof; 
	_test_eof865: ( cs) = 865; goto _test_eof; 
	_test_eof866: ( cs) = 866; goto _test_eof; 
	_test_eof867: ( cs) = 867; goto _test_eof; 
	_test_eof868: ( cs) = 868; goto _test_eof; 
	_test_eof869: ( cs) = 869; goto _test_eof; 
	_test_eof1868: ( cs) = 1868; goto _test_eof; 
	_test_eof870: ( cs) = 870; goto _test_eof; 
	_test_eof1869: ( cs) = 1869; goto _test_eof; 
	_test_eof871: ( cs) = 871; goto _test_eof; 
	_test_eof872: ( cs) = 872; goto _test_eof; 
	_test_eof873: ( cs) = 873; goto _test_eof; 
	_test_eof874: ( cs) = 874; goto _test_eof; 
	_test_eof875: ( cs) = 875; goto _test_eof; 
	_test_eof876: ( cs) = 876; goto _test_eof; 
	_test_eof877: ( cs) = 877; goto _test_eof; 
	_test_eof878: ( cs) = 878; goto _test_eof; 
	_test_eof879: ( cs) = 879; goto _test_eof; 
	_test_eof880: ( cs) = 880; goto _test_eof; 
	_test_eof881: ( cs) = 881; goto _test_eof; 
	_test_eof882: ( cs) = 882; goto _test_eof; 
	_test_eof883: ( cs) = 883; goto _test_eof; 
	_test_eof884: ( cs) = 884; goto _test_eof; 
	_test_eof885: ( cs) = 885; goto _test_eof; 
	_test_eof886: ( cs) = 886; goto _test_eof; 
	_test_eof887: ( cs) = 887; goto _test_eof; 
	_test_eof1870: ( cs) = 1870; goto _test_eof; 
	_test_eof888: ( cs) = 888; goto _test_eof; 
	_test_eof889: ( cs) = 889; goto _test_eof; 
	_test_eof890: ( cs) = 890; goto _test_eof; 
	_test_eof891: ( cs) = 891; goto _test_eof; 
	_test_eof892: ( cs) = 892; goto _test_eof; 
	_test_eof893: ( cs) = 893; goto _test_eof; 
	_test_eof894: ( cs) = 894; goto _test_eof; 
	_test_eof895: ( cs) = 895; goto _test_eof; 
	_test_eof896: ( cs) = 896; goto _test_eof; 
	_test_eof897: ( cs) = 897; goto _test_eof; 
	_test_eof898: ( cs) = 898; goto _test_eof; 
	_test_eof899: ( cs) = 899; goto _test_eof; 
	_test_eof900: ( cs) = 900; goto _test_eof; 
	_test_eof901: ( cs) = 901; goto _test_eof; 
	_test_eof902: ( cs) = 902; goto _test_eof; 
	_test_eof903: ( cs) = 903; goto _test_eof; 
	_test_eof904: ( cs) = 904; goto _test_eof; 
	_test_eof905: ( cs) = 905; goto _test_eof; 
	_test_eof906: ( cs) = 906; goto _test_eof; 
	_test_eof907: ( cs) = 907; goto _test_eof; 
	_test_eof908: ( cs) = 908; goto _test_eof; 
	_test_eof909: ( cs) = 909; goto _test_eof; 
	_test_eof910: ( cs) = 910; goto _test_eof; 
	_test_eof911: ( cs) = 911; goto _test_eof; 
	_test_eof912: ( cs) = 912; goto _test_eof; 
	_test_eof913: ( cs) = 913; goto _test_eof; 
	_test_eof914: ( cs) = 914; goto _test_eof; 
	_test_eof915: ( cs) = 915; goto _test_eof; 
	_test_eof916: ( cs) = 916; goto _test_eof; 
	_test_eof917: ( cs) = 917; goto _test_eof; 
	_test_eof918: ( cs) = 918; goto _test_eof; 
	_test_eof919: ( cs) = 919; goto _test_eof; 
	_test_eof920: ( cs) = 920; goto _test_eof; 
	_test_eof921: ( cs) = 921; goto _test_eof; 
	_test_eof922: ( cs) = 922; goto _test_eof; 
	_test_eof923: ( cs) = 923; goto _test_eof; 
	_test_eof924: ( cs) = 924; goto _test_eof; 
	_test_eof925: ( cs) = 925; goto _test_eof; 
	_test_eof926: ( cs) = 926; goto _test_eof; 
	_test_eof927: ( cs) = 927; goto _test_eof; 
	_test_eof928: ( cs) = 928; goto _test_eof; 
	_test_eof929: ( cs) = 929; goto _test_eof; 
	_test_eof930: ( cs) = 930; goto _test_eof; 
	_test_eof931: ( cs) = 931; goto _test_eof; 
	_test_eof932: ( cs) = 932; goto _test_eof; 
	_test_eof933: ( cs) = 933; goto _test_eof; 
	_test_eof934: ( cs) = 934; goto _test_eof; 
	_test_eof935: ( cs) = 935; goto _test_eof; 
	_test_eof936: ( cs) = 936; goto _test_eof; 
	_test_eof937: ( cs) = 937; goto _test_eof; 
	_test_eof938: ( cs) = 938; goto _test_eof; 
	_test_eof939: ( cs) = 939; goto _test_eof; 
	_test_eof940: ( cs) = 940; goto _test_eof; 
	_test_eof941: ( cs) = 941; goto _test_eof; 
	_test_eof942: ( cs) = 942; goto _test_eof; 
	_test_eof943: ( cs) = 943; goto _test_eof; 
	_test_eof944: ( cs) = 944; goto _test_eof; 
	_test_eof945: ( cs) = 945; goto _test_eof; 
	_test_eof946: ( cs) = 946; goto _test_eof; 
	_test_eof947: ( cs) = 947; goto _test_eof; 
	_test_eof948: ( cs) = 948; goto _test_eof; 
	_test_eof949: ( cs) = 949; goto _test_eof; 
	_test_eof950: ( cs) = 950; goto _test_eof; 
	_test_eof951: ( cs) = 951; goto _test_eof; 
	_test_eof952: ( cs) = 952; goto _test_eof; 
	_test_eof953: ( cs) = 953; goto _test_eof; 
	_test_eof954: ( cs) = 954; goto _test_eof; 
	_test_eof955: ( cs) = 955; goto _test_eof; 
	_test_eof956: ( cs) = 956; goto _test_eof; 
	_test_eof957: ( cs) = 957; goto _test_eof; 
	_test_eof958: ( cs) = 958; goto _test_eof; 
	_test_eof959: ( cs) = 959; goto _test_eof; 
	_test_eof960: ( cs) = 960; goto _test_eof; 
	_test_eof961: ( cs) = 961; goto _test_eof; 
	_test_eof962: ( cs) = 962; goto _test_eof; 
	_test_eof963: ( cs) = 963; goto _test_eof; 
	_test_eof964: ( cs) = 964; goto _test_eof; 
	_test_eof965: ( cs) = 965; goto _test_eof; 
	_test_eof966: ( cs) = 966; goto _test_eof; 
	_test_eof967: ( cs) = 967; goto _test_eof; 
	_test_eof968: ( cs) = 968; goto _test_eof; 
	_test_eof969: ( cs) = 969; goto _test_eof; 
	_test_eof970: ( cs) = 970; goto _test_eof; 
	_test_eof971: ( cs) = 971; goto _test_eof; 
	_test_eof972: ( cs) = 972; goto _test_eof; 
	_test_eof973: ( cs) = 973; goto _test_eof; 
	_test_eof974: ( cs) = 974; goto _test_eof; 
	_test_eof975: ( cs) = 975; goto _test_eof; 
	_test_eof976: ( cs) = 976; goto _test_eof; 
	_test_eof977: ( cs) = 977; goto _test_eof; 
	_test_eof978: ( cs) = 978; goto _test_eof; 
	_test_eof979: ( cs) = 979; goto _test_eof; 
	_test_eof980: ( cs) = 980; goto _test_eof; 
	_test_eof981: ( cs) = 981; goto _test_eof; 
	_test_eof982: ( cs) = 982; goto _test_eof; 
	_test_eof983: ( cs) = 983; goto _test_eof; 
	_test_eof984: ( cs) = 984; goto _test_eof; 
	_test_eof985: ( cs) = 985; goto _test_eof; 
	_test_eof986: ( cs) = 986; goto _test_eof; 
	_test_eof987: ( cs) = 987; goto _test_eof; 
	_test_eof988: ( cs) = 988; goto _test_eof; 
	_test_eof989: ( cs) = 989; goto _test_eof; 
	_test_eof990: ( cs) = 990; goto _test_eof; 
	_test_eof991: ( cs) = 991; goto _test_eof; 
	_test_eof992: ( cs) = 992; goto _test_eof; 
	_test_eof993: ( cs) = 993; goto _test_eof; 
	_test_eof994: ( cs) = 994; goto _test_eof; 
	_test_eof995: ( cs) = 995; goto _test_eof; 
	_test_eof996: ( cs) = 996; goto _test_eof; 
	_test_eof997: ( cs) = 997; goto _test_eof; 
	_test_eof998: ( cs) = 998; goto _test_eof; 
	_test_eof999: ( cs) = 999; goto _test_eof; 
	_test_eof1000: ( cs) = 1000; goto _test_eof; 
	_test_eof1001: ( cs) = 1001; goto _test_eof; 
	_test_eof1002: ( cs) = 1002; goto _test_eof; 
	_test_eof1003: ( cs) = 1003; goto _test_eof; 
	_test_eof1004: ( cs) = 1004; goto _test_eof; 
	_test_eof1005: ( cs) = 1005; goto _test_eof; 
	_test_eof1006: ( cs) = 1006; goto _test_eof; 
	_test_eof1007: ( cs) = 1007; goto _test_eof; 
	_test_eof1008: ( cs) = 1008; goto _test_eof; 
	_test_eof1009: ( cs) = 1009; goto _test_eof; 
	_test_eof1010: ( cs) = 1010; goto _test_eof; 
	_test_eof1011: ( cs) = 1011; goto _test_eof; 
	_test_eof1012: ( cs) = 1012; goto _test_eof; 
	_test_eof1013: ( cs) = 1013; goto _test_eof; 
	_test_eof1014: ( cs) = 1014; goto _test_eof; 
	_test_eof1015: ( cs) = 1015; goto _test_eof; 
	_test_eof1016: ( cs) = 1016; goto _test_eof; 
	_test_eof1017: ( cs) = 1017; goto _test_eof; 
	_test_eof1018: ( cs) = 1018; goto _test_eof; 
	_test_eof1019: ( cs) = 1019; goto _test_eof; 
	_test_eof1020: ( cs) = 1020; goto _test_eof; 
	_test_eof1021: ( cs) = 1021; goto _test_eof; 
	_test_eof1022: ( cs) = 1022; goto _test_eof; 
	_test_eof1023: ( cs) = 1023; goto _test_eof; 
	_test_eof1024: ( cs) = 1024; goto _test_eof; 
	_test_eof1025: ( cs) = 1025; goto _test_eof; 
	_test_eof1026: ( cs) = 1026; goto _test_eof; 
	_test_eof1027: ( cs) = 1027; goto _test_eof; 
	_test_eof1028: ( cs) = 1028; goto _test_eof; 
	_test_eof1029: ( cs) = 1029; goto _test_eof; 
	_test_eof1030: ( cs) = 1030; goto _test_eof; 
	_test_eof1031: ( cs) = 1031; goto _test_eof; 
	_test_eof1032: ( cs) = 1032; goto _test_eof; 
	_test_eof1033: ( cs) = 1033; goto _test_eof; 
	_test_eof1034: ( cs) = 1034; goto _test_eof; 
	_test_eof1035: ( cs) = 1035; goto _test_eof; 
	_test_eof1036: ( cs) = 1036; goto _test_eof; 
	_test_eof1037: ( cs) = 1037; goto _test_eof; 
	_test_eof1038: ( cs) = 1038; goto _test_eof; 
	_test_eof1039: ( cs) = 1039; goto _test_eof; 
	_test_eof1040: ( cs) = 1040; goto _test_eof; 
	_test_eof1041: ( cs) = 1041; goto _test_eof; 
	_test_eof1042: ( cs) = 1042; goto _test_eof; 
	_test_eof1043: ( cs) = 1043; goto _test_eof; 
	_test_eof1044: ( cs) = 1044; goto _test_eof; 
	_test_eof1045: ( cs) = 1045; goto _test_eof; 
	_test_eof1046: ( cs) = 1046; goto _test_eof; 
	_test_eof1047: ( cs) = 1047; goto _test_eof; 
	_test_eof1048: ( cs) = 1048; goto _test_eof; 
	_test_eof1871: ( cs) = 1871; goto _test_eof; 
	_test_eof1049: ( cs) = 1049; goto _test_eof; 
	_test_eof1050: ( cs) = 1050; goto _test_eof; 
	_test_eof1872: ( cs) = 1872; goto _test_eof; 
	_test_eof1051: ( cs) = 1051; goto _test_eof; 
	_test_eof1052: ( cs) = 1052; goto _test_eof; 
	_test_eof1053: ( cs) = 1053; goto _test_eof; 
	_test_eof1054: ( cs) = 1054; goto _test_eof; 
	_test_eof1055: ( cs) = 1055; goto _test_eof; 
	_test_eof1056: ( cs) = 1056; goto _test_eof; 
	_test_eof1057: ( cs) = 1057; goto _test_eof; 
	_test_eof1058: ( cs) = 1058; goto _test_eof; 
	_test_eof1059: ( cs) = 1059; goto _test_eof; 
	_test_eof1060: ( cs) = 1060; goto _test_eof; 
	_test_eof1061: ( cs) = 1061; goto _test_eof; 
	_test_eof1062: ( cs) = 1062; goto _test_eof; 
	_test_eof1063: ( cs) = 1063; goto _test_eof; 
	_test_eof1064: ( cs) = 1064; goto _test_eof; 
	_test_eof1065: ( cs) = 1065; goto _test_eof; 
	_test_eof1066: ( cs) = 1066; goto _test_eof; 
	_test_eof1067: ( cs) = 1067; goto _test_eof; 
	_test_eof1873: ( cs) = 1873; goto _test_eof; 
	_test_eof1068: ( cs) = 1068; goto _test_eof; 
	_test_eof1069: ( cs) = 1069; goto _test_eof; 
	_test_eof1070: ( cs) = 1070; goto _test_eof; 
	_test_eof1071: ( cs) = 1071; goto _test_eof; 
	_test_eof1072: ( cs) = 1072; goto _test_eof; 
	_test_eof1073: ( cs) = 1073; goto _test_eof; 
	_test_eof1074: ( cs) = 1074; goto _test_eof; 
	_test_eof1075: ( cs) = 1075; goto _test_eof; 
	_test_eof1076: ( cs) = 1076; goto _test_eof; 
	_test_eof1077: ( cs) = 1077; goto _test_eof; 
	_test_eof1078: ( cs) = 1078; goto _test_eof; 
	_test_eof1079: ( cs) = 1079; goto _test_eof; 
	_test_eof1080: ( cs) = 1080; goto _test_eof; 
	_test_eof1081: ( cs) = 1081; goto _test_eof; 
	_test_eof1082: ( cs) = 1082; goto _test_eof; 
	_test_eof1083: ( cs) = 1083; goto _test_eof; 
	_test_eof1084: ( cs) = 1084; goto _test_eof; 
	_test_eof1085: ( cs) = 1085; goto _test_eof; 
	_test_eof1086: ( cs) = 1086; goto _test_eof; 
	_test_eof1087: ( cs) = 1087; goto _test_eof; 
	_test_eof1088: ( cs) = 1088; goto _test_eof; 
	_test_eof1089: ( cs) = 1089; goto _test_eof; 
	_test_eof1090: ( cs) = 1090; goto _test_eof; 
	_test_eof1091: ( cs) = 1091; goto _test_eof; 
	_test_eof1092: ( cs) = 1092; goto _test_eof; 
	_test_eof1093: ( cs) = 1093; goto _test_eof; 
	_test_eof1094: ( cs) = 1094; goto _test_eof; 
	_test_eof1095: ( cs) = 1095; goto _test_eof; 
	_test_eof1096: ( cs) = 1096; goto _test_eof; 
	_test_eof1097: ( cs) = 1097; goto _test_eof; 
	_test_eof1098: ( cs) = 1098; goto _test_eof; 
	_test_eof1099: ( cs) = 1099; goto _test_eof; 
	_test_eof1100: ( cs) = 1100; goto _test_eof; 
	_test_eof1101: ( cs) = 1101; goto _test_eof; 
	_test_eof1102: ( cs) = 1102; goto _test_eof; 
	_test_eof1103: ( cs) = 1103; goto _test_eof; 
	_test_eof1104: ( cs) = 1104; goto _test_eof; 
	_test_eof1105: ( cs) = 1105; goto _test_eof; 
	_test_eof1106: ( cs) = 1106; goto _test_eof; 
	_test_eof1107: ( cs) = 1107; goto _test_eof; 
	_test_eof1108: ( cs) = 1108; goto _test_eof; 
	_test_eof1109: ( cs) = 1109; goto _test_eof; 
	_test_eof1110: ( cs) = 1110; goto _test_eof; 
	_test_eof1874: ( cs) = 1874; goto _test_eof; 
	_test_eof1111: ( cs) = 1111; goto _test_eof; 
	_test_eof1112: ( cs) = 1112; goto _test_eof; 
	_test_eof1113: ( cs) = 1113; goto _test_eof; 
	_test_eof1114: ( cs) = 1114; goto _test_eof; 
	_test_eof1115: ( cs) = 1115; goto _test_eof; 
	_test_eof1116: ( cs) = 1116; goto _test_eof; 
	_test_eof1117: ( cs) = 1117; goto _test_eof; 
	_test_eof1118: ( cs) = 1118; goto _test_eof; 
	_test_eof1119: ( cs) = 1119; goto _test_eof; 
	_test_eof1120: ( cs) = 1120; goto _test_eof; 
	_test_eof1121: ( cs) = 1121; goto _test_eof; 
	_test_eof1122: ( cs) = 1122; goto _test_eof; 
	_test_eof1123: ( cs) = 1123; goto _test_eof; 
	_test_eof1124: ( cs) = 1124; goto _test_eof; 
	_test_eof1125: ( cs) = 1125; goto _test_eof; 
	_test_eof1126: ( cs) = 1126; goto _test_eof; 
	_test_eof1127: ( cs) = 1127; goto _test_eof; 
	_test_eof1128: ( cs) = 1128; goto _test_eof; 
	_test_eof1129: ( cs) = 1129; goto _test_eof; 
	_test_eof1130: ( cs) = 1130; goto _test_eof; 
	_test_eof1875: ( cs) = 1875; goto _test_eof; 
	_test_eof1131: ( cs) = 1131; goto _test_eof; 
	_test_eof1132: ( cs) = 1132; goto _test_eof; 
	_test_eof1133: ( cs) = 1133; goto _test_eof; 
	_test_eof1134: ( cs) = 1134; goto _test_eof; 
	_test_eof1135: ( cs) = 1135; goto _test_eof; 
	_test_eof1136: ( cs) = 1136; goto _test_eof; 
	_test_eof1137: ( cs) = 1137; goto _test_eof; 
	_test_eof1138: ( cs) = 1138; goto _test_eof; 
	_test_eof1139: ( cs) = 1139; goto _test_eof; 
	_test_eof1140: ( cs) = 1140; goto _test_eof; 
	_test_eof1141: ( cs) = 1141; goto _test_eof; 
	_test_eof1142: ( cs) = 1142; goto _test_eof; 
	_test_eof1143: ( cs) = 1143; goto _test_eof; 
	_test_eof1144: ( cs) = 1144; goto _test_eof; 
	_test_eof1145: ( cs) = 1145; goto _test_eof; 
	_test_eof1146: ( cs) = 1146; goto _test_eof; 
	_test_eof1147: ( cs) = 1147; goto _test_eof; 
	_test_eof1148: ( cs) = 1148; goto _test_eof; 
	_test_eof1149: ( cs) = 1149; goto _test_eof; 
	_test_eof1150: ( cs) = 1150; goto _test_eof; 
	_test_eof1151: ( cs) = 1151; goto _test_eof; 
	_test_eof1152: ( cs) = 1152; goto _test_eof; 
	_test_eof1153: ( cs) = 1153; goto _test_eof; 
	_test_eof1876: ( cs) = 1876; goto _test_eof; 
	_test_eof1154: ( cs) = 1154; goto _test_eof; 
	_test_eof1155: ( cs) = 1155; goto _test_eof; 
	_test_eof1156: ( cs) = 1156; goto _test_eof; 
	_test_eof1157: ( cs) = 1157; goto _test_eof; 
	_test_eof1158: ( cs) = 1158; goto _test_eof; 
	_test_eof1159: ( cs) = 1159; goto _test_eof; 
	_test_eof1160: ( cs) = 1160; goto _test_eof; 
	_test_eof1161: ( cs) = 1161; goto _test_eof; 
	_test_eof1162: ( cs) = 1162; goto _test_eof; 
	_test_eof1163: ( cs) = 1163; goto _test_eof; 
	_test_eof1164: ( cs) = 1164; goto _test_eof; 
	_test_eof1165: ( cs) = 1165; goto _test_eof; 
	_test_eof1166: ( cs) = 1166; goto _test_eof; 
	_test_eof1167: ( cs) = 1167; goto _test_eof; 
	_test_eof1168: ( cs) = 1168; goto _test_eof; 
	_test_eof1169: ( cs) = 1169; goto _test_eof; 
	_test_eof1170: ( cs) = 1170; goto _test_eof; 
	_test_eof1171: ( cs) = 1171; goto _test_eof; 
	_test_eof1172: ( cs) = 1172; goto _test_eof; 
	_test_eof1173: ( cs) = 1173; goto _test_eof; 
	_test_eof1174: ( cs) = 1174; goto _test_eof; 
	_test_eof1175: ( cs) = 1175; goto _test_eof; 
	_test_eof1176: ( cs) = 1176; goto _test_eof; 
	_test_eof1177: ( cs) = 1177; goto _test_eof; 
	_test_eof1178: ( cs) = 1178; goto _test_eof; 
	_test_eof1179: ( cs) = 1179; goto _test_eof; 
	_test_eof1180: ( cs) = 1180; goto _test_eof; 
	_test_eof1877: ( cs) = 1877; goto _test_eof; 
	_test_eof1181: ( cs) = 1181; goto _test_eof; 
	_test_eof1182: ( cs) = 1182; goto _test_eof; 
	_test_eof1183: ( cs) = 1183; goto _test_eof; 
	_test_eof1184: ( cs) = 1184; goto _test_eof; 
	_test_eof1185: ( cs) = 1185; goto _test_eof; 
	_test_eof1186: ( cs) = 1186; goto _test_eof; 
	_test_eof1187: ( cs) = 1187; goto _test_eof; 
	_test_eof1188: ( cs) = 1188; goto _test_eof; 
	_test_eof1189: ( cs) = 1189; goto _test_eof; 
	_test_eof1190: ( cs) = 1190; goto _test_eof; 
	_test_eof1191: ( cs) = 1191; goto _test_eof; 
	_test_eof1192: ( cs) = 1192; goto _test_eof; 
	_test_eof1193: ( cs) = 1193; goto _test_eof; 
	_test_eof1194: ( cs) = 1194; goto _test_eof; 
	_test_eof1195: ( cs) = 1195; goto _test_eof; 
	_test_eof1196: ( cs) = 1196; goto _test_eof; 
	_test_eof1197: ( cs) = 1197; goto _test_eof; 
	_test_eof1198: ( cs) = 1198; goto _test_eof; 
	_test_eof1199: ( cs) = 1199; goto _test_eof; 
	_test_eof1200: ( cs) = 1200; goto _test_eof; 
	_test_eof1201: ( cs) = 1201; goto _test_eof; 
	_test_eof1202: ( cs) = 1202; goto _test_eof; 
	_test_eof1203: ( cs) = 1203; goto _test_eof; 
	_test_eof1204: ( cs) = 1204; goto _test_eof; 
	_test_eof1205: ( cs) = 1205; goto _test_eof; 
	_test_eof1206: ( cs) = 1206; goto _test_eof; 
	_test_eof1207: ( cs) = 1207; goto _test_eof; 
	_test_eof1208: ( cs) = 1208; goto _test_eof; 
	_test_eof1209: ( cs) = 1209; goto _test_eof; 
	_test_eof1210: ( cs) = 1210; goto _test_eof; 
	_test_eof1211: ( cs) = 1211; goto _test_eof; 
	_test_eof1212: ( cs) = 1212; goto _test_eof; 
	_test_eof1213: ( cs) = 1213; goto _test_eof; 
	_test_eof1214: ( cs) = 1214; goto _test_eof; 
	_test_eof1215: ( cs) = 1215; goto _test_eof; 
	_test_eof1216: ( cs) = 1216; goto _test_eof; 
	_test_eof1217: ( cs) = 1217; goto _test_eof; 
	_test_eof1218: ( cs) = 1218; goto _test_eof; 
	_test_eof1219: ( cs) = 1219; goto _test_eof; 
	_test_eof1220: ( cs) = 1220; goto _test_eof; 
	_test_eof1221: ( cs) = 1221; goto _test_eof; 
	_test_eof1222: ( cs) = 1222; goto _test_eof; 
	_test_eof1223: ( cs) = 1223; goto _test_eof; 
	_test_eof1224: ( cs) = 1224; goto _test_eof; 
	_test_eof1225: ( cs) = 1225; goto _test_eof; 
	_test_eof1226: ( cs) = 1226; goto _test_eof; 
	_test_eof1227: ( cs) = 1227; goto _test_eof; 
	_test_eof1228: ( cs) = 1228; goto _test_eof; 
	_test_eof1229: ( cs) = 1229; goto _test_eof; 
	_test_eof1230: ( cs) = 1230; goto _test_eof; 
	_test_eof1231: ( cs) = 1231; goto _test_eof; 
	_test_eof1232: ( cs) = 1232; goto _test_eof; 
	_test_eof1233: ( cs) = 1233; goto _test_eof; 
	_test_eof1234: ( cs) = 1234; goto _test_eof; 
	_test_eof1235: ( cs) = 1235; goto _test_eof; 
	_test_eof1236: ( cs) = 1236; goto _test_eof; 
	_test_eof1237: ( cs) = 1237; goto _test_eof; 
	_test_eof1238: ( cs) = 1238; goto _test_eof; 
	_test_eof1239: ( cs) = 1239; goto _test_eof; 
	_test_eof1240: ( cs) = 1240; goto _test_eof; 
	_test_eof1241: ( cs) = 1241; goto _test_eof; 
	_test_eof1242: ( cs) = 1242; goto _test_eof; 
	_test_eof1243: ( cs) = 1243; goto _test_eof; 
	_test_eof1244: ( cs) = 1244; goto _test_eof; 
	_test_eof1245: ( cs) = 1245; goto _test_eof; 
	_test_eof1246: ( cs) = 1246; goto _test_eof; 
	_test_eof1247: ( cs) = 1247; goto _test_eof; 
	_test_eof1248: ( cs) = 1248; goto _test_eof; 
	_test_eof1249: ( cs) = 1249; goto _test_eof; 
	_test_eof1250: ( cs) = 1250; goto _test_eof; 
	_test_eof1251: ( cs) = 1251; goto _test_eof; 
	_test_eof1252: ( cs) = 1252; goto _test_eof; 
	_test_eof1253: ( cs) = 1253; goto _test_eof; 
	_test_eof1254: ( cs) = 1254; goto _test_eof; 
	_test_eof1255: ( cs) = 1255; goto _test_eof; 
	_test_eof1256: ( cs) = 1256; goto _test_eof; 
	_test_eof1257: ( cs) = 1257; goto _test_eof; 
	_test_eof1258: ( cs) = 1258; goto _test_eof; 
	_test_eof1259: ( cs) = 1259; goto _test_eof; 
	_test_eof1260: ( cs) = 1260; goto _test_eof; 
	_test_eof1261: ( cs) = 1261; goto _test_eof; 
	_test_eof1262: ( cs) = 1262; goto _test_eof; 
	_test_eof1878: ( cs) = 1878; goto _test_eof; 
	_test_eof1879: ( cs) = 1879; goto _test_eof; 
	_test_eof1263: ( cs) = 1263; goto _test_eof; 
	_test_eof1264: ( cs) = 1264; goto _test_eof; 
	_test_eof1265: ( cs) = 1265; goto _test_eof; 
	_test_eof1266: ( cs) = 1266; goto _test_eof; 
	_test_eof1267: ( cs) = 1267; goto _test_eof; 
	_test_eof1268: ( cs) = 1268; goto _test_eof; 
	_test_eof1269: ( cs) = 1269; goto _test_eof; 
	_test_eof1270: ( cs) = 1270; goto _test_eof; 
	_test_eof1271: ( cs) = 1271; goto _test_eof; 
	_test_eof1272: ( cs) = 1272; goto _test_eof; 
	_test_eof1273: ( cs) = 1273; goto _test_eof; 
	_test_eof1274: ( cs) = 1274; goto _test_eof; 
	_test_eof1275: ( cs) = 1275; goto _test_eof; 
	_test_eof1276: ( cs) = 1276; goto _test_eof; 
	_test_eof1277: ( cs) = 1277; goto _test_eof; 
	_test_eof1278: ( cs) = 1278; goto _test_eof; 
	_test_eof1279: ( cs) = 1279; goto _test_eof; 
	_test_eof1280: ( cs) = 1280; goto _test_eof; 
	_test_eof1281: ( cs) = 1281; goto _test_eof; 
	_test_eof1282: ( cs) = 1282; goto _test_eof; 
	_test_eof1283: ( cs) = 1283; goto _test_eof; 
	_test_eof1284: ( cs) = 1284; goto _test_eof; 
	_test_eof1285: ( cs) = 1285; goto _test_eof; 
	_test_eof1286: ( cs) = 1286; goto _test_eof; 
	_test_eof1287: ( cs) = 1287; goto _test_eof; 
	_test_eof1288: ( cs) = 1288; goto _test_eof; 
	_test_eof1289: ( cs) = 1289; goto _test_eof; 
	_test_eof1290: ( cs) = 1290; goto _test_eof; 
	_test_eof1291: ( cs) = 1291; goto _test_eof; 
	_test_eof1292: ( cs) = 1292; goto _test_eof; 
	_test_eof1293: ( cs) = 1293; goto _test_eof; 
	_test_eof1294: ( cs) = 1294; goto _test_eof; 
	_test_eof1295: ( cs) = 1295; goto _test_eof; 
	_test_eof1296: ( cs) = 1296; goto _test_eof; 
	_test_eof1297: ( cs) = 1297; goto _test_eof; 
	_test_eof1298: ( cs) = 1298; goto _test_eof; 
	_test_eof1299: ( cs) = 1299; goto _test_eof; 
	_test_eof1300: ( cs) = 1300; goto _test_eof; 
	_test_eof1301: ( cs) = 1301; goto _test_eof; 
	_test_eof1302: ( cs) = 1302; goto _test_eof; 
	_test_eof1303: ( cs) = 1303; goto _test_eof; 
	_test_eof1304: ( cs) = 1304; goto _test_eof; 
	_test_eof1305: ( cs) = 1305; goto _test_eof; 
	_test_eof1880: ( cs) = 1880; goto _test_eof; 
	_test_eof1306: ( cs) = 1306; goto _test_eof; 
	_test_eof1307: ( cs) = 1307; goto _test_eof; 
	_test_eof1308: ( cs) = 1308; goto _test_eof; 
	_test_eof1309: ( cs) = 1309; goto _test_eof; 
	_test_eof1310: ( cs) = 1310; goto _test_eof; 
	_test_eof1311: ( cs) = 1311; goto _test_eof; 
	_test_eof1312: ( cs) = 1312; goto _test_eof; 
	_test_eof1313: ( cs) = 1313; goto _test_eof; 
	_test_eof1314: ( cs) = 1314; goto _test_eof; 
	_test_eof1315: ( cs) = 1315; goto _test_eof; 
	_test_eof1316: ( cs) = 1316; goto _test_eof; 
	_test_eof1317: ( cs) = 1317; goto _test_eof; 
	_test_eof1318: ( cs) = 1318; goto _test_eof; 
	_test_eof1319: ( cs) = 1319; goto _test_eof; 
	_test_eof1320: ( cs) = 1320; goto _test_eof; 
	_test_eof1321: ( cs) = 1321; goto _test_eof; 
	_test_eof1322: ( cs) = 1322; goto _test_eof; 
	_test_eof1323: ( cs) = 1323; goto _test_eof; 
	_test_eof1324: ( cs) = 1324; goto _test_eof; 
	_test_eof1325: ( cs) = 1325; goto _test_eof; 
	_test_eof1326: ( cs) = 1326; goto _test_eof; 
	_test_eof1327: ( cs) = 1327; goto _test_eof; 
	_test_eof1328: ( cs) = 1328; goto _test_eof; 
	_test_eof1329: ( cs) = 1329; goto _test_eof; 
	_test_eof1330: ( cs) = 1330; goto _test_eof; 
	_test_eof1331: ( cs) = 1331; goto _test_eof; 
	_test_eof1332: ( cs) = 1332; goto _test_eof; 
	_test_eof1333: ( cs) = 1333; goto _test_eof; 
	_test_eof1334: ( cs) = 1334; goto _test_eof; 
	_test_eof1335: ( cs) = 1335; goto _test_eof; 
	_test_eof1336: ( cs) = 1336; goto _test_eof; 
	_test_eof1337: ( cs) = 1337; goto _test_eof; 
	_test_eof1338: ( cs) = 1338; goto _test_eof; 
	_test_eof1339: ( cs) = 1339; goto _test_eof; 
	_test_eof1881: ( cs) = 1881; goto _test_eof; 
	_test_eof1340: ( cs) = 1340; goto _test_eof; 
	_test_eof1341: ( cs) = 1341; goto _test_eof; 
	_test_eof1882: ( cs) = 1882; goto _test_eof; 
	_test_eof1342: ( cs) = 1342; goto _test_eof; 
	_test_eof1343: ( cs) = 1343; goto _test_eof; 
	_test_eof1344: ( cs) = 1344; goto _test_eof; 
	_test_eof1345: ( cs) = 1345; goto _test_eof; 
	_test_eof1883: ( cs) = 1883; goto _test_eof; 
	_test_eof1346: ( cs) = 1346; goto _test_eof; 
	_test_eof1347: ( cs) = 1347; goto _test_eof; 
	_test_eof1348: ( cs) = 1348; goto _test_eof; 
	_test_eof1349: ( cs) = 1349; goto _test_eof; 
	_test_eof1350: ( cs) = 1350; goto _test_eof; 
	_test_eof1351: ( cs) = 1351; goto _test_eof; 
	_test_eof1352: ( cs) = 1352; goto _test_eof; 
	_test_eof1353: ( cs) = 1353; goto _test_eof; 
	_test_eof1354: ( cs) = 1354; goto _test_eof; 
	_test_eof1355: ( cs) = 1355; goto _test_eof; 
	_test_eof1884: ( cs) = 1884; goto _test_eof; 
	_test_eof1356: ( cs) = 1356; goto _test_eof; 
	_test_eof1357: ( cs) = 1357; goto _test_eof; 
	_test_eof1358: ( cs) = 1358; goto _test_eof; 
	_test_eof1359: ( cs) = 1359; goto _test_eof; 
	_test_eof1360: ( cs) = 1360; goto _test_eof; 
	_test_eof1361: ( cs) = 1361; goto _test_eof; 
	_test_eof1362: ( cs) = 1362; goto _test_eof; 
	_test_eof1363: ( cs) = 1363; goto _test_eof; 
	_test_eof1364: ( cs) = 1364; goto _test_eof; 
	_test_eof1365: ( cs) = 1365; goto _test_eof; 
	_test_eof1366: ( cs) = 1366; goto _test_eof; 
	_test_eof1367: ( cs) = 1367; goto _test_eof; 
	_test_eof1368: ( cs) = 1368; goto _test_eof; 
	_test_eof1369: ( cs) = 1369; goto _test_eof; 
	_test_eof1370: ( cs) = 1370; goto _test_eof; 
	_test_eof1371: ( cs) = 1371; goto _test_eof; 
	_test_eof1372: ( cs) = 1372; goto _test_eof; 
	_test_eof1373: ( cs) = 1373; goto _test_eof; 
	_test_eof1374: ( cs) = 1374; goto _test_eof; 
	_test_eof1375: ( cs) = 1375; goto _test_eof; 
	_test_eof1885: ( cs) = 1885; goto _test_eof; 
	_test_eof1886: ( cs) = 1886; goto _test_eof; 
	_test_eof1376: ( cs) = 1376; goto _test_eof; 
	_test_eof1377: ( cs) = 1377; goto _test_eof; 
	_test_eof1378: ( cs) = 1378; goto _test_eof; 
	_test_eof1379: ( cs) = 1379; goto _test_eof; 
	_test_eof1380: ( cs) = 1380; goto _test_eof; 
	_test_eof1381: ( cs) = 1381; goto _test_eof; 
	_test_eof1382: ( cs) = 1382; goto _test_eof; 
	_test_eof1383: ( cs) = 1383; goto _test_eof; 
	_test_eof1384: ( cs) = 1384; goto _test_eof; 
	_test_eof1385: ( cs) = 1385; goto _test_eof; 
	_test_eof1386: ( cs) = 1386; goto _test_eof; 
	_test_eof1387: ( cs) = 1387; goto _test_eof; 
	_test_eof1887: ( cs) = 1887; goto _test_eof; 
	_test_eof1888: ( cs) = 1888; goto _test_eof; 
	_test_eof1889: ( cs) = 1889; goto _test_eof; 
	_test_eof1890: ( cs) = 1890; goto _test_eof; 
	_test_eof1388: ( cs) = 1388; goto _test_eof; 
	_test_eof1389: ( cs) = 1389; goto _test_eof; 
	_test_eof1390: ( cs) = 1390; goto _test_eof; 
	_test_eof1391: ( cs) = 1391; goto _test_eof; 
	_test_eof1392: ( cs) = 1392; goto _test_eof; 
	_test_eof1393: ( cs) = 1393; goto _test_eof; 
	_test_eof1394: ( cs) = 1394; goto _test_eof; 
	_test_eof1395: ( cs) = 1395; goto _test_eof; 
	_test_eof1396: ( cs) = 1396; goto _test_eof; 
	_test_eof1397: ( cs) = 1397; goto _test_eof; 
	_test_eof1398: ( cs) = 1398; goto _test_eof; 
	_test_eof1399: ( cs) = 1399; goto _test_eof; 
	_test_eof1400: ( cs) = 1400; goto _test_eof; 
	_test_eof1401: ( cs) = 1401; goto _test_eof; 
	_test_eof1402: ( cs) = 1402; goto _test_eof; 
	_test_eof1403: ( cs) = 1403; goto _test_eof; 
	_test_eof1404: ( cs) = 1404; goto _test_eof; 
	_test_eof1405: ( cs) = 1405; goto _test_eof; 
	_test_eof1891: ( cs) = 1891; goto _test_eof; 
	_test_eof1892: ( cs) = 1892; goto _test_eof; 
	_test_eof1893: ( cs) = 1893; goto _test_eof; 
	_test_eof1894: ( cs) = 1894; goto _test_eof; 
	_test_eof1406: ( cs) = 1406; goto _test_eof; 
	_test_eof1407: ( cs) = 1407; goto _test_eof; 
	_test_eof1408: ( cs) = 1408; goto _test_eof; 
	_test_eof1409: ( cs) = 1409; goto _test_eof; 
	_test_eof1410: ( cs) = 1410; goto _test_eof; 
	_test_eof1411: ( cs) = 1411; goto _test_eof; 
	_test_eof1412: ( cs) = 1412; goto _test_eof; 
	_test_eof1413: ( cs) = 1413; goto _test_eof; 
	_test_eof1414: ( cs) = 1414; goto _test_eof; 
	_test_eof1415: ( cs) = 1415; goto _test_eof; 
	_test_eof1416: ( cs) = 1416; goto _test_eof; 
	_test_eof1417: ( cs) = 1417; goto _test_eof; 
	_test_eof1418: ( cs) = 1418; goto _test_eof; 
	_test_eof1419: ( cs) = 1419; goto _test_eof; 
	_test_eof1420: ( cs) = 1420; goto _test_eof; 
	_test_eof1421: ( cs) = 1421; goto _test_eof; 
	_test_eof1422: ( cs) = 1422; goto _test_eof; 
	_test_eof1423: ( cs) = 1423; goto _test_eof; 
	_test_eof1424: ( cs) = 1424; goto _test_eof; 
	_test_eof1425: ( cs) = 1425; goto _test_eof; 
	_test_eof1426: ( cs) = 1426; goto _test_eof; 
	_test_eof1427: ( cs) = 1427; goto _test_eof; 
	_test_eof1428: ( cs) = 1428; goto _test_eof; 
	_test_eof1429: ( cs) = 1429; goto _test_eof; 
	_test_eof1430: ( cs) = 1430; goto _test_eof; 
	_test_eof1431: ( cs) = 1431; goto _test_eof; 
	_test_eof1432: ( cs) = 1432; goto _test_eof; 
	_test_eof1433: ( cs) = 1433; goto _test_eof; 
	_test_eof1434: ( cs) = 1434; goto _test_eof; 
	_test_eof1435: ( cs) = 1435; goto _test_eof; 
	_test_eof1436: ( cs) = 1436; goto _test_eof; 
	_test_eof1437: ( cs) = 1437; goto _test_eof; 
	_test_eof1438: ( cs) = 1438; goto _test_eof; 
	_test_eof1439: ( cs) = 1439; goto _test_eof; 
	_test_eof1440: ( cs) = 1440; goto _test_eof; 
	_test_eof1441: ( cs) = 1441; goto _test_eof; 
	_test_eof1442: ( cs) = 1442; goto _test_eof; 
	_test_eof1443: ( cs) = 1443; goto _test_eof; 
	_test_eof1444: ( cs) = 1444; goto _test_eof; 
	_test_eof1445: ( cs) = 1445; goto _test_eof; 
	_test_eof1446: ( cs) = 1446; goto _test_eof; 
	_test_eof1447: ( cs) = 1447; goto _test_eof; 
	_test_eof1448: ( cs) = 1448; goto _test_eof; 
	_test_eof1449: ( cs) = 1449; goto _test_eof; 
	_test_eof1450: ( cs) = 1450; goto _test_eof; 
	_test_eof1451: ( cs) = 1451; goto _test_eof; 
	_test_eof1452: ( cs) = 1452; goto _test_eof; 
	_test_eof1453: ( cs) = 1453; goto _test_eof; 
	_test_eof1454: ( cs) = 1454; goto _test_eof; 
	_test_eof1455: ( cs) = 1455; goto _test_eof; 
	_test_eof1456: ( cs) = 1456; goto _test_eof; 
	_test_eof1457: ( cs) = 1457; goto _test_eof; 
	_test_eof1458: ( cs) = 1458; goto _test_eof; 
	_test_eof1459: ( cs) = 1459; goto _test_eof; 
	_test_eof1460: ( cs) = 1460; goto _test_eof; 
	_test_eof1461: ( cs) = 1461; goto _test_eof; 
	_test_eof1462: ( cs) = 1462; goto _test_eof; 
	_test_eof1463: ( cs) = 1463; goto _test_eof; 
	_test_eof1464: ( cs) = 1464; goto _test_eof; 
	_test_eof1465: ( cs) = 1465; goto _test_eof; 
	_test_eof1466: ( cs) = 1466; goto _test_eof; 
	_test_eof1467: ( cs) = 1467; goto _test_eof; 
	_test_eof1468: ( cs) = 1468; goto _test_eof; 
	_test_eof1469: ( cs) = 1469; goto _test_eof; 
	_test_eof1470: ( cs) = 1470; goto _test_eof; 
	_test_eof1471: ( cs) = 1471; goto _test_eof; 
	_test_eof1472: ( cs) = 1472; goto _test_eof; 
	_test_eof1473: ( cs) = 1473; goto _test_eof; 
	_test_eof1474: ( cs) = 1474; goto _test_eof; 
	_test_eof1475: ( cs) = 1475; goto _test_eof; 
	_test_eof1476: ( cs) = 1476; goto _test_eof; 
	_test_eof1477: ( cs) = 1477; goto _test_eof; 
	_test_eof1478: ( cs) = 1478; goto _test_eof; 
	_test_eof1479: ( cs) = 1479; goto _test_eof; 
	_test_eof1480: ( cs) = 1480; goto _test_eof; 
	_test_eof1481: ( cs) = 1481; goto _test_eof; 
	_test_eof1482: ( cs) = 1482; goto _test_eof; 
	_test_eof1483: ( cs) = 1483; goto _test_eof; 
	_test_eof1484: ( cs) = 1484; goto _test_eof; 
	_test_eof1485: ( cs) = 1485; goto _test_eof; 
	_test_eof1486: ( cs) = 1486; goto _test_eof; 
	_test_eof1487: ( cs) = 1487; goto _test_eof; 
	_test_eof1488: ( cs) = 1488; goto _test_eof; 
	_test_eof1489: ( cs) = 1489; goto _test_eof; 
	_test_eof1490: ( cs) = 1490; goto _test_eof; 
	_test_eof1491: ( cs) = 1491; goto _test_eof; 
	_test_eof1492: ( cs) = 1492; goto _test_eof; 
	_test_eof1493: ( cs) = 1493; goto _test_eof; 
	_test_eof1494: ( cs) = 1494; goto _test_eof; 
	_test_eof1495: ( cs) = 1495; goto _test_eof; 
	_test_eof1496: ( cs) = 1496; goto _test_eof; 
	_test_eof1497: ( cs) = 1497; goto _test_eof; 
	_test_eof1498: ( cs) = 1498; goto _test_eof; 
	_test_eof1499: ( cs) = 1499; goto _test_eof; 
	_test_eof1500: ( cs) = 1500; goto _test_eof; 
	_test_eof1501: ( cs) = 1501; goto _test_eof; 
	_test_eof1502: ( cs) = 1502; goto _test_eof; 
	_test_eof1503: ( cs) = 1503; goto _test_eof; 
	_test_eof1504: ( cs) = 1504; goto _test_eof; 
	_test_eof1505: ( cs) = 1505; goto _test_eof; 
	_test_eof1506: ( cs) = 1506; goto _test_eof; 
	_test_eof1507: ( cs) = 1507; goto _test_eof; 
	_test_eof1508: ( cs) = 1508; goto _test_eof; 
	_test_eof1509: ( cs) = 1509; goto _test_eof; 
	_test_eof1510: ( cs) = 1510; goto _test_eof; 
	_test_eof1511: ( cs) = 1511; goto _test_eof; 
	_test_eof1512: ( cs) = 1512; goto _test_eof; 
	_test_eof1513: ( cs) = 1513; goto _test_eof; 
	_test_eof1514: ( cs) = 1514; goto _test_eof; 
	_test_eof1515: ( cs) = 1515; goto _test_eof; 
	_test_eof1516: ( cs) = 1516; goto _test_eof; 
	_test_eof1517: ( cs) = 1517; goto _test_eof; 
	_test_eof1895: ( cs) = 1895; goto _test_eof; 
	_test_eof1518: ( cs) = 1518; goto _test_eof; 
	_test_eof1519: ( cs) = 1519; goto _test_eof; 
	_test_eof1520: ( cs) = 1520; goto _test_eof; 
	_test_eof1521: ( cs) = 1521; goto _test_eof; 
	_test_eof1522: ( cs) = 1522; goto _test_eof; 
	_test_eof1523: ( cs) = 1523; goto _test_eof; 
	_test_eof1524: ( cs) = 1524; goto _test_eof; 
	_test_eof1525: ( cs) = 1525; goto _test_eof; 
	_test_eof1526: ( cs) = 1526; goto _test_eof; 
	_test_eof1527: ( cs) = 1527; goto _test_eof; 
	_test_eof1528: ( cs) = 1528; goto _test_eof; 
	_test_eof1529: ( cs) = 1529; goto _test_eof; 
	_test_eof1530: ( cs) = 1530; goto _test_eof; 
	_test_eof1531: ( cs) = 1531; goto _test_eof; 
	_test_eof1532: ( cs) = 1532; goto _test_eof; 
	_test_eof1533: ( cs) = 1533; goto _test_eof; 
	_test_eof1534: ( cs) = 1534; goto _test_eof; 
	_test_eof1535: ( cs) = 1535; goto _test_eof; 
	_test_eof1536: ( cs) = 1536; goto _test_eof; 
	_test_eof1537: ( cs) = 1537; goto _test_eof; 
	_test_eof1538: ( cs) = 1538; goto _test_eof; 
	_test_eof1539: ( cs) = 1539; goto _test_eof; 
	_test_eof1540: ( cs) = 1540; goto _test_eof; 
	_test_eof1541: ( cs) = 1541; goto _test_eof; 
	_test_eof1542: ( cs) = 1542; goto _test_eof; 
	_test_eof1543: ( cs) = 1543; goto _test_eof; 
	_test_eof1544: ( cs) = 1544; goto _test_eof; 
	_test_eof1545: ( cs) = 1545; goto _test_eof; 
	_test_eof1546: ( cs) = 1546; goto _test_eof; 
	_test_eof1547: ( cs) = 1547; goto _test_eof; 
	_test_eof1548: ( cs) = 1548; goto _test_eof; 
	_test_eof1549: ( cs) = 1549; goto _test_eof; 
	_test_eof1550: ( cs) = 1550; goto _test_eof; 
	_test_eof1551: ( cs) = 1551; goto _test_eof; 
	_test_eof1552: ( cs) = 1552; goto _test_eof; 
	_test_eof1553: ( cs) = 1553; goto _test_eof; 
	_test_eof1554: ( cs) = 1554; goto _test_eof; 
	_test_eof1555: ( cs) = 1555; goto _test_eof; 
	_test_eof1556: ( cs) = 1556; goto _test_eof; 
	_test_eof1557: ( cs) = 1557; goto _test_eof; 
	_test_eof1558: ( cs) = 1558; goto _test_eof; 
	_test_eof1559: ( cs) = 1559; goto _test_eof; 
	_test_eof1560: ( cs) = 1560; goto _test_eof; 
	_test_eof1561: ( cs) = 1561; goto _test_eof; 
	_test_eof1562: ( cs) = 1562; goto _test_eof; 
	_test_eof1563: ( cs) = 1563; goto _test_eof; 
	_test_eof1564: ( cs) = 1564; goto _test_eof; 
	_test_eof1565: ( cs) = 1565; goto _test_eof; 
	_test_eof1566: ( cs) = 1566; goto _test_eof; 
	_test_eof1567: ( cs) = 1567; goto _test_eof; 
	_test_eof1568: ( cs) = 1568; goto _test_eof; 
	_test_eof1569: ( cs) = 1569; goto _test_eof; 
	_test_eof1570: ( cs) = 1570; goto _test_eof; 
	_test_eof1571: ( cs) = 1571; goto _test_eof; 
	_test_eof1572: ( cs) = 1572; goto _test_eof; 
	_test_eof1573: ( cs) = 1573; goto _test_eof; 
	_test_eof1574: ( cs) = 1574; goto _test_eof; 
	_test_eof1575: ( cs) = 1575; goto _test_eof; 
	_test_eof1576: ( cs) = 1576; goto _test_eof; 
	_test_eof1577: ( cs) = 1577; goto _test_eof; 
	_test_eof1578: ( cs) = 1578; goto _test_eof; 
	_test_eof1579: ( cs) = 1579; goto _test_eof; 
	_test_eof1580: ( cs) = 1580; goto _test_eof; 
	_test_eof1581: ( cs) = 1581; goto _test_eof; 
	_test_eof1582: ( cs) = 1582; goto _test_eof; 
	_test_eof1583: ( cs) = 1583; goto _test_eof; 
	_test_eof1584: ( cs) = 1584; goto _test_eof; 
	_test_eof1585: ( cs) = 1585; goto _test_eof; 
	_test_eof1586: ( cs) = 1586; goto _test_eof; 
	_test_eof1587: ( cs) = 1587; goto _test_eof; 
	_test_eof1588: ( cs) = 1588; goto _test_eof; 
	_test_eof1589: ( cs) = 1589; goto _test_eof; 
	_test_eof1590: ( cs) = 1590; goto _test_eof; 
	_test_eof1591: ( cs) = 1591; goto _test_eof; 
	_test_eof1592: ( cs) = 1592; goto _test_eof; 
	_test_eof1593: ( cs) = 1593; goto _test_eof; 
	_test_eof1594: ( cs) = 1594; goto _test_eof; 
	_test_eof1595: ( cs) = 1595; goto _test_eof; 
	_test_eof1596: ( cs) = 1596; goto _test_eof; 
	_test_eof1597: ( cs) = 1597; goto _test_eof; 
	_test_eof1598: ( cs) = 1598; goto _test_eof; 
	_test_eof1599: ( cs) = 1599; goto _test_eof; 
	_test_eof1600: ( cs) = 1600; goto _test_eof; 
	_test_eof1601: ( cs) = 1601; goto _test_eof; 
	_test_eof1602: ( cs) = 1602; goto _test_eof; 
	_test_eof1603: ( cs) = 1603; goto _test_eof; 
	_test_eof1604: ( cs) = 1604; goto _test_eof; 
	_test_eof1605: ( cs) = 1605; goto _test_eof; 
	_test_eof1606: ( cs) = 1606; goto _test_eof; 
	_test_eof1607: ( cs) = 1607; goto _test_eof; 
	_test_eof1608: ( cs) = 1608; goto _test_eof; 
	_test_eof1609: ( cs) = 1609; goto _test_eof; 
	_test_eof1610: ( cs) = 1610; goto _test_eof; 
	_test_eof1611: ( cs) = 1611; goto _test_eof; 
	_test_eof1612: ( cs) = 1612; goto _test_eof; 
	_test_eof1613: ( cs) = 1613; goto _test_eof; 
	_test_eof1614: ( cs) = 1614; goto _test_eof; 
	_test_eof1615: ( cs) = 1615; goto _test_eof; 
	_test_eof1616: ( cs) = 1616; goto _test_eof; 
	_test_eof1617: ( cs) = 1617; goto _test_eof; 
	_test_eof1618: ( cs) = 1618; goto _test_eof; 
	_test_eof1619: ( cs) = 1619; goto _test_eof; 
	_test_eof1620: ( cs) = 1620; goto _test_eof; 
	_test_eof1621: ( cs) = 1621; goto _test_eof; 
	_test_eof1622: ( cs) = 1622; goto _test_eof; 
	_test_eof1623: ( cs) = 1623; goto _test_eof; 
	_test_eof1624: ( cs) = 1624; goto _test_eof; 
	_test_eof1625: ( cs) = 1625; goto _test_eof; 
	_test_eof1626: ( cs) = 1626; goto _test_eof; 
	_test_eof1627: ( cs) = 1627; goto _test_eof; 
	_test_eof1628: ( cs) = 1628; goto _test_eof; 
	_test_eof1629: ( cs) = 1629; goto _test_eof; 

	_test_eof: {}
	if ( ( p) == ( eof) )
	{
	switch ( ( cs) ) {
	case 1631: goto tr0;
	case 1: goto tr0;
	case 1632: goto tr2026;
	case 2: goto tr3;
	case 1633: goto tr0;
	case 3: goto tr0;
	case 4: goto tr0;
	case 5: goto tr0;
	case 6: goto tr0;
	case 7: goto tr0;
	case 8: goto tr0;
	case 9: goto tr0;
	case 10: goto tr0;
	case 11: goto tr0;
	case 12: goto tr0;
	case 13: goto tr0;
	case 14: goto tr0;
	case 15: goto tr0;
	case 16: goto tr0;
	case 1634: goto tr2027;
	case 17: goto tr0;
	case 18: goto tr0;
	case 19: goto tr0;
	case 20: goto tr0;
	case 21: goto tr0;
	case 22: goto tr0;
	case 1635: goto tr2028;
	case 23: goto tr0;
	case 24: goto tr0;
	case 25: goto tr0;
	case 26: goto tr0;
	case 27: goto tr0;
	case 28: goto tr0;
	case 29: goto tr0;
	case 30: goto tr0;
	case 31: goto tr0;
	case 32: goto tr0;
	case 33: goto tr0;
	case 34: goto tr0;
	case 35: goto tr0;
	case 36: goto tr0;
	case 37: goto tr0;
	case 38: goto tr0;
	case 39: goto tr0;
	case 40: goto tr0;
	case 41: goto tr0;
	case 42: goto tr0;
	case 43: goto tr0;
	case 44: goto tr0;
	case 1636: goto tr2029;
	case 45: goto tr0;
	case 46: goto tr0;
	case 47: goto tr0;
	case 48: goto tr0;
	case 49: goto tr0;
	case 50: goto tr0;
	case 51: goto tr0;
	case 52: goto tr0;
	case 53: goto tr0;
	case 54: goto tr0;
	case 55: goto tr0;
	case 56: goto tr3;
	case 57: goto tr3;
	case 58: goto tr3;
	case 59: goto tr3;
	case 1637: goto tr2030;
	case 60: goto tr3;
	case 61: goto tr3;
	case 62: goto tr3;
	case 63: goto tr3;
	case 64: goto tr3;
	case 65: goto tr3;
	case 66: goto tr3;
	case 67: goto tr3;
	case 68: goto tr3;
	case 69: goto tr3;
	case 70: goto tr3;
	case 71: goto tr3;
	case 72: goto tr3;
	case 73: goto tr3;
	case 74: goto tr3;
	case 1638: goto tr2026;
	case 1639: goto tr2026;
	case 75: goto tr3;
	case 1640: goto tr2031;
	case 1641: goto tr2031;
	case 76: goto tr3;
	case 1642: goto tr2026;
	case 77: goto tr3;
	case 78: goto tr3;
	case 79: goto tr3;
	case 1643: goto tr2034;
	case 1644: goto tr2026;
	case 80: goto tr3;
	case 81: goto tr3;
	case 82: goto tr3;
	case 83: goto tr3;
	case 84: goto tr3;
	case 85: goto tr3;
	case 86: goto tr3;
	case 87: goto tr3;
	case 88: goto tr3;
	case 89: goto tr3;
	case 1645: goto tr2026;
	case 90: goto tr3;
	case 91: goto tr3;
	case 92: goto tr3;
	case 93: goto tr3;
	case 94: goto tr3;
	case 95: goto tr3;
	case 96: goto tr3;
	case 97: goto tr3;
	case 98: goto tr3;
	case 99: goto tr3;
	case 1646: goto tr2043;
	case 100: goto tr3;
	case 101: goto tr3;
	case 102: goto tr3;
	case 103: goto tr3;
	case 104: goto tr3;
	case 105: goto tr3;
	case 106: goto tr3;
	case 1647: goto tr2044;
	case 107: goto tr130;
	case 1648: goto tr2045;
	case 108: goto tr133;
	case 109: goto tr3;
	case 110: goto tr3;
	case 111: goto tr3;
	case 112: goto tr3;
	case 113: goto tr3;
	case 114: goto tr3;
	case 115: goto tr3;
	case 116: goto tr3;
	case 1649: goto tr2046;
	case 117: goto tr3;
	case 1650: goto tr2048;
	case 118: goto tr3;
	case 119: goto tr3;
	case 120: goto tr3;
	case 121: goto tr3;
	case 122: goto tr3;
	case 123: goto tr3;
	case 124: goto tr3;
	case 1651: goto tr2049;
	case 125: goto tr157;
	case 126: goto tr3;
	case 127: goto tr3;
	case 128: goto tr3;
	case 129: goto tr3;
	case 130: goto tr3;
	case 131: goto tr3;
	case 132: goto tr3;
	case 1652: goto tr2050;
	case 133: goto tr3;
	case 134: goto tr3;
	case 135: goto tr3;
	case 1653: goto tr2026;
	case 136: goto tr3;
	case 137: goto tr3;
	case 138: goto tr3;
	case 139: goto tr3;
	case 140: goto tr3;
	case 141: goto tr3;
	case 142: goto tr3;
	case 143: goto tr3;
	case 144: goto tr3;
	case 145: goto tr3;
	case 146: goto tr3;
	case 147: goto tr3;
	case 148: goto tr3;
	case 149: goto tr3;
	case 150: goto tr3;
	case 151: goto tr3;
	case 152: goto tr3;
	case 153: goto tr3;
	case 154: goto tr3;
	case 155: goto tr3;
	case 156: goto tr3;
	case 157: goto tr3;
	case 158: goto tr3;
	case 159: goto tr3;
	case 160: goto tr3;
	case 161: goto tr3;
	case 162: goto tr3;
	case 163: goto tr3;
	case 164: goto tr3;
	case 165: goto tr3;
	case 166: goto tr3;
	case 167: goto tr3;
	case 168: goto tr3;
	case 169: goto tr3;
	case 170: goto tr3;
	case 171: goto tr3;
	case 172: goto tr3;
	case 173: goto tr3;
	case 1654: goto tr2026;
	case 1655: goto tr2026;
	case 1657: goto tr2061;
	case 174: goto tr206;
	case 175: goto tr206;
	case 176: goto tr206;
	case 177: goto tr206;
	case 178: goto tr206;
	case 179: goto tr206;
	case 180: goto tr206;
	case 181: goto tr206;
	case 182: goto tr206;
	case 183: goto tr206;
	case 184: goto tr206;
	case 185: goto tr206;
	case 186: goto tr206;
	case 187: goto tr206;
	case 188: goto tr206;
	case 189: goto tr206;
	case 190: goto tr206;
	case 191: goto tr206;
	case 192: goto tr206;
	case 1658: goto tr2061;
	case 193: goto tr206;
	case 194: goto tr206;
	case 195: goto tr206;
	case 196: goto tr206;
	case 197: goto tr206;
	case 198: goto tr206;
	case 199: goto tr206;
	case 200: goto tr206;
	case 201: goto tr206;
	case 1660: goto tr2102;
	case 1661: goto tr2103;
	case 202: goto tr234;
	case 203: goto tr234;
	case 204: goto tr237;
	case 1662: goto tr2102;
	case 1663: goto tr2102;
	case 1664: goto tr234;
	case 205: goto tr234;
	case 1665: goto tr2102;
	case 206: goto tr241;
	case 1666: goto tr2105;
	case 207: goto tr243;
	case 208: goto tr243;
	case 209: goto tr243;
	case 210: goto tr243;
	case 211: goto tr234;
	case 212: goto tr234;
	case 213: goto tr234;
	case 214: goto tr234;
	case 215: goto tr234;
	case 216: goto tr234;
	case 217: goto tr234;
	case 218: goto tr234;
	case 219: goto tr234;
	case 1667: goto tr2112;
	case 220: goto tr243;
	case 221: goto tr234;
	case 222: goto tr234;
	case 223: goto tr234;
	case 224: goto tr234;
	case 225: goto tr234;
	case 1668: goto tr2113;
	case 226: goto tr234;
	case 227: goto tr234;
	case 228: goto tr234;
	case 229: goto tr234;
	case 230: goto tr234;
	case 231: goto tr243;
	case 232: goto tr234;
	case 233: goto tr234;
	case 234: goto tr243;
	case 235: goto tr234;
	case 236: goto tr234;
	case 237: goto tr234;
	case 238: goto tr234;
	case 239: goto tr234;
	case 240: goto tr234;
	case 241: goto tr234;
	case 242: goto tr234;
	case 243: goto tr234;
	case 244: goto tr243;
	case 245: goto tr243;
	case 246: goto tr243;
	case 1669: goto tr2114;
	case 247: goto tr243;
	case 248: goto tr243;
	case 249: goto tr243;
	case 250: goto tr243;
	case 251: goto tr243;
	case 252: goto tr243;
	case 253: goto tr243;
	case 254: goto tr243;
	case 255: goto tr243;
	case 256: goto tr243;
	case 257: goto tr243;
	case 258: goto tr243;
	case 259: goto tr243;
	case 260: goto tr243;
	case 261: goto tr243;
	case 262: goto tr243;
	case 263: goto tr243;
	case 264: goto tr243;
	case 265: goto tr243;
	case 266: goto tr243;
	case 267: goto tr243;
	case 268: goto tr243;
	case 269: goto tr243;
	case 270: goto tr243;
	case 271: goto tr243;
	case 272: goto tr243;
	case 273: goto tr243;
	case 274: goto tr243;
	case 275: goto tr243;
	case 276: goto tr243;
	case 277: goto tr243;
	case 278: goto tr243;
	case 279: goto tr243;
	case 280: goto tr243;
	case 281: goto tr243;
	case 1670: goto tr2115;
	case 282: goto tr326;
	case 283: goto tr326;
	case 284: goto tr326;
	case 285: goto tr234;
	case 286: goto tr326;
	case 287: goto tr326;
	case 288: goto tr326;
	case 289: goto tr234;
	case 290: goto tr243;
	case 291: goto tr243;
	case 1671: goto tr2118;
	case 1672: goto tr2118;
	case 292: goto tr243;
	case 293: goto tr243;
	case 294: goto tr243;
	case 295: goto tr234;
	case 296: goto tr234;
	case 297: goto tr234;
	case 298: goto tr234;
	case 299: goto tr234;
	case 300: goto tr234;
	case 301: goto tr234;
	case 302: goto tr234;
	case 303: goto tr243;
	case 304: goto tr243;
	case 305: goto tr243;
	case 306: goto tr243;
	case 307: goto tr243;
	case 308: goto tr243;
	case 309: goto tr243;
	case 310: goto tr243;
	case 311: goto tr243;
	case 312: goto tr243;
	case 313: goto tr243;
	case 314: goto tr243;
	case 315: goto tr243;
	case 316: goto tr243;
	case 317: goto tr243;
	case 318: goto tr243;
	case 319: goto tr243;
	case 320: goto tr243;
	case 321: goto tr243;
	case 322: goto tr243;
	case 323: goto tr243;
	case 324: goto tr243;
	case 325: goto tr243;
	case 326: goto tr243;
	case 327: goto tr243;
	case 328: goto tr243;
	case 329: goto tr243;
	case 330: goto tr243;
	case 331: goto tr243;
	case 332: goto tr243;
	case 333: goto tr243;
	case 1673: goto tr2114;
	case 334: goto tr243;
	case 335: goto tr243;
	case 336: goto tr243;
	case 337: goto tr243;
	case 338: goto tr243;
	case 339: goto tr243;
	case 340: goto tr243;
	case 341: goto tr243;
	case 342: goto tr243;
	case 343: goto tr243;
	case 344: goto tr243;
	case 345: goto tr243;
	case 346: goto tr243;
	case 347: goto tr243;
	case 348: goto tr243;
	case 349: goto tr243;
	case 350: goto tr243;
	case 351: goto tr243;
	case 352: goto tr243;
	case 353: goto tr243;
	case 354: goto tr243;
	case 355: goto tr243;
	case 356: goto tr243;
	case 357: goto tr243;
	case 358: goto tr243;
	case 359: goto tr243;
	case 360: goto tr243;
	case 361: goto tr243;
	case 362: goto tr243;
	case 363: goto tr243;
	case 364: goto tr243;
	case 365: goto tr243;
	case 366: goto tr243;
	case 367: goto tr243;
	case 368: goto tr243;
	case 369: goto tr243;
	case 370: goto tr243;
	case 371: goto tr243;
	case 372: goto tr243;
	case 373: goto tr243;
	case 374: goto tr243;
	case 375: goto tr243;
	case 376: goto tr243;
	case 377: goto tr243;
	case 378: goto tr243;
	case 379: goto tr243;
	case 380: goto tr243;
	case 381: goto tr243;
	case 382: goto tr243;
	case 1674: goto tr2102;
	case 383: goto tr241;
	case 384: goto tr241;
	case 385: goto tr241;
	case 1675: goto tr2122;
	case 386: goto tr454;
	case 387: goto tr454;
	case 388: goto tr454;
	case 389: goto tr454;
	case 390: goto tr454;
	case 391: goto tr454;
	case 392: goto tr454;
	case 393: goto tr454;
	case 394: goto tr454;
	case 395: goto tr454;
	case 396: goto tr454;
	case 1676: goto tr2122;
	case 397: goto tr454;
	case 398: goto tr454;
	case 399: goto tr454;
	case 400: goto tr454;
	case 401: goto tr454;
	case 402: goto tr454;
	case 403: goto tr454;
	case 404: goto tr454;
	case 405: goto tr454;
	case 406: goto tr454;
	case 407: goto tr454;
	case 408: goto tr234;
	case 409: goto tr234;
	case 1677: goto tr2122;
	case 410: goto tr234;
	case 411: goto tr234;
	case 412: goto tr234;
	case 413: goto tr234;
	case 414: goto tr234;
	case 415: goto tr234;
	case 416: goto tr234;
	case 417: goto tr234;
	case 418: goto tr234;
	case 419: goto tr241;
	case 420: goto tr241;
	case 421: goto tr241;
	case 422: goto tr241;
	case 423: goto tr241;
	case 424: goto tr241;
	case 425: goto tr241;
	case 426: goto tr241;
	case 427: goto tr241;
	case 428: goto tr241;
	case 429: goto tr241;
	case 430: goto tr234;
	case 431: goto tr234;
	case 1678: goto tr2122;
	case 432: goto tr234;
	case 433: goto tr234;
	case 434: goto tr234;
	case 435: goto tr234;
	case 436: goto tr234;
	case 437: goto tr234;
	case 438: goto tr234;
	case 439: goto tr234;
	case 440: goto tr234;
	case 441: goto tr234;
	case 442: goto tr234;
	case 1679: goto tr2122;
	case 443: goto tr241;
	case 444: goto tr241;
	case 445: goto tr241;
	case 446: goto tr241;
	case 447: goto tr241;
	case 448: goto tr241;
	case 449: goto tr241;
	case 450: goto tr241;
	case 451: goto tr241;
	case 452: goto tr241;
	case 453: goto tr241;
	case 1680: goto tr2102;
	case 454: goto tr241;
	case 455: goto tr241;
	case 456: goto tr241;
	case 457: goto tr241;
	case 458: goto tr241;
	case 459: goto tr241;
	case 460: goto tr241;
	case 461: goto tr241;
	case 462: goto tr241;
	case 463: goto tr241;
	case 464: goto tr241;
	case 465: goto tr241;
	case 466: goto tr241;
	case 467: goto tr241;
	case 468: goto tr241;
	case 469: goto tr241;
	case 470: goto tr241;
	case 471: goto tr241;
	case 472: goto tr241;
	case 473: goto tr241;
	case 474: goto tr241;
	case 475: goto tr241;
	case 476: goto tr241;
	case 477: goto tr241;
	case 478: goto tr241;
	case 479: goto tr241;
	case 480: goto tr241;
	case 481: goto tr241;
	case 482: goto tr241;
	case 483: goto tr241;
	case 484: goto tr241;
	case 485: goto tr241;
	case 486: goto tr241;
	case 487: goto tr241;
	case 488: goto tr241;
	case 489: goto tr241;
	case 490: goto tr241;
	case 491: goto tr241;
	case 492: goto tr241;
	case 493: goto tr241;
	case 494: goto tr241;
	case 495: goto tr241;
	case 496: goto tr241;
	case 497: goto tr241;
	case 498: goto tr241;
	case 499: goto tr241;
	case 500: goto tr241;
	case 1681: goto tr2103;
	case 501: goto tr237;
	case 502: goto tr237;
	case 503: goto tr234;
	case 504: goto tr234;
	case 505: goto tr234;
	case 506: goto tr234;
	case 507: goto tr234;
	case 508: goto tr234;
	case 509: goto tr234;
	case 1682: goto tr2134;
	case 1683: goto tr2136;
	case 510: goto tr234;
	case 511: goto tr234;
	case 512: goto tr234;
	case 513: goto tr234;
	case 1684: goto tr2138;
	case 1685: goto tr2140;
	case 514: goto tr234;
	case 515: goto tr234;
	case 516: goto tr237;
	case 517: goto tr237;
	case 518: goto tr237;
	case 519: goto tr237;
	case 520: goto tr237;
	case 521: goto tr237;
	case 522: goto tr237;
	case 1686: goto tr2134;
	case 1687: goto tr2136;
	case 523: goto tr237;
	case 524: goto tr237;
	case 525: goto tr237;
	case 526: goto tr237;
	case 527: goto tr237;
	case 528: goto tr237;
	case 529: goto tr237;
	case 530: goto tr237;
	case 531: goto tr237;
	case 532: goto tr237;
	case 533: goto tr237;
	case 534: goto tr237;
	case 535: goto tr237;
	case 536: goto tr237;
	case 537: goto tr237;
	case 538: goto tr237;
	case 539: goto tr237;
	case 540: goto tr237;
	case 541: goto tr237;
	case 542: goto tr237;
	case 543: goto tr237;
	case 544: goto tr237;
	case 545: goto tr237;
	case 546: goto tr237;
	case 547: goto tr237;
	case 548: goto tr237;
	case 549: goto tr237;
	case 550: goto tr237;
	case 551: goto tr237;
	case 552: goto tr237;
	case 553: goto tr237;
	case 554: goto tr237;
	case 555: goto tr234;
	case 556: goto tr234;
	case 557: goto tr234;
	case 558: goto tr234;
	case 559: goto tr234;
	case 560: goto tr234;
	case 561: goto tr234;
	case 562: goto tr234;
	case 563: goto tr234;
	case 564: goto tr234;
	case 565: goto tr234;
	case 1688: goto tr2144;
	case 1689: goto tr2146;
	case 566: goto tr234;
	case 1690: goto tr2148;
	case 1691: goto tr2150;
	case 567: goto tr234;
	case 568: goto tr234;
	case 569: goto tr234;
	case 570: goto tr234;
	case 571: goto tr234;
	case 572: goto tr234;
	case 573: goto tr234;
	case 574: goto tr234;
	case 1692: goto tr2148;
	case 1693: goto tr2150;
	case 575: goto tr234;
	case 1694: goto tr2148;
	case 576: goto tr234;
	case 577: goto tr234;
	case 578: goto tr234;
	case 579: goto tr234;
	case 580: goto tr234;
	case 581: goto tr234;
	case 582: goto tr234;
	case 583: goto tr234;
	case 584: goto tr234;
	case 585: goto tr234;
	case 586: goto tr234;
	case 587: goto tr234;
	case 588: goto tr234;
	case 589: goto tr234;
	case 590: goto tr234;
	case 591: goto tr234;
	case 592: goto tr234;
	case 593: goto tr234;
	case 1695: goto tr2148;
	case 594: goto tr234;
	case 595: goto tr234;
	case 596: goto tr234;
	case 597: goto tr234;
	case 598: goto tr234;
	case 599: goto tr234;
	case 600: goto tr234;
	case 601: goto tr234;
	case 602: goto tr234;
	case 1696: goto tr2103;
	case 1697: goto tr2103;
	case 1698: goto tr2103;
	case 1699: goto tr2103;
	case 1700: goto tr2103;
	case 603: goto tr237;
	case 604: goto tr237;
	case 1701: goto tr2162;
	case 1702: goto tr2103;
	case 1703: goto tr2103;
	case 1704: goto tr2103;
	case 1705: goto tr2103;
	case 1706: goto tr2103;
	case 605: goto tr237;
	case 606: goto tr237;
	case 1707: goto tr2169;
	case 607: goto tr237;
	case 608: goto tr237;
	case 609: goto tr237;
	case 610: goto tr237;
	case 611: goto tr237;
	case 612: goto tr237;
	case 613: goto tr237;
	case 614: goto tr237;
	case 615: goto tr237;
	case 1708: goto tr2171;
	case 1709: goto tr2103;
	case 1710: goto tr2103;
	case 1711: goto tr2103;
	case 1712: goto tr2103;
	case 616: goto tr237;
	case 617: goto tr237;
	case 618: goto tr237;
	case 619: goto tr237;
	case 620: goto tr237;
	case 621: goto tr237;
	case 622: goto tr237;
	case 623: goto tr237;
	case 624: goto tr237;
	case 625: goto tr237;
	case 1713: goto tr2177;
	case 1714: goto tr2103;
	case 1715: goto tr2103;
	case 1716: goto tr2103;
	case 626: goto tr237;
	case 627: goto tr237;
	case 1717: goto tr2183;
	case 1718: goto tr2103;
	case 1719: goto tr2103;
	case 628: goto tr237;
	case 629: goto tr237;
	case 1720: goto tr2187;
	case 1721: goto tr2103;
	case 1722: goto tr2103;
	case 1723: goto tr2103;
	case 1724: goto tr2103;
	case 1725: goto tr2103;
	case 1726: goto tr2103;
	case 1727: goto tr2103;
	case 630: goto tr237;
	case 631: goto tr237;
	case 1728: goto tr2197;
	case 1729: goto tr2103;
	case 1730: goto tr2103;
	case 632: goto tr237;
	case 633: goto tr237;
	case 1731: goto tr2201;
	case 1732: goto tr2103;
	case 1733: goto tr2103;
	case 1734: goto tr2103;
	case 1735: goto tr2103;
	case 1736: goto tr2103;
	case 634: goto tr237;
	case 635: goto tr237;
	case 1737: goto tr2209;
	case 636: goto tr791;
	case 1738: goto tr2212;
	case 1739: goto tr2103;
	case 1740: goto tr2103;
	case 637: goto tr237;
	case 638: goto tr237;
	case 1741: goto tr2216;
	case 1742: goto tr2103;
	case 1743: goto tr2103;
	case 1744: goto tr2103;
	case 1745: goto tr2103;
	case 639: goto tr237;
	case 640: goto tr237;
	case 1746: goto tr2223;
	case 1747: goto tr2103;
	case 1748: goto tr2103;
	case 1749: goto tr2103;
	case 1750: goto tr2103;
	case 641: goto tr237;
	case 642: goto tr237;
	case 643: goto tr237;
	case 1751: goto tr2230;
	case 644: goto tr237;
	case 645: goto tr237;
	case 646: goto tr237;
	case 647: goto tr237;
	case 648: goto tr237;
	case 649: goto tr237;
	case 650: goto tr237;
	case 651: goto tr237;
	case 652: goto tr237;
	case 653: goto tr237;
	case 654: goto tr237;
	case 1752: goto tr2232;
	case 655: goto tr812;
	case 656: goto tr812;
	case 1753: goto tr2235;
	case 1754: goto tr2103;
	case 1755: goto tr2103;
	case 1756: goto tr2103;
	case 1757: goto tr2103;
	case 1758: goto tr2103;
	case 1759: goto tr2103;
	case 1760: goto tr2103;
	case 1761: goto tr2103;
	case 657: goto tr237;
	case 658: goto tr237;
	case 659: goto tr237;
	case 660: goto tr237;
	case 661: goto tr237;
	case 662: goto tr237;
	case 663: goto tr237;
	case 664: goto tr234;
	case 665: goto tr234;
	case 1762: goto tr2245;
	case 666: goto tr234;
	case 667: goto tr234;
	case 668: goto tr234;
	case 669: goto tr234;
	case 670: goto tr234;
	case 671: goto tr234;
	case 672: goto tr234;
	case 673: goto tr234;
	case 674: goto tr234;
	case 675: goto tr234;
	case 1763: goto tr2245;
	case 676: goto tr838;
	case 677: goto tr838;
	case 678: goto tr838;
	case 679: goto tr838;
	case 680: goto tr838;
	case 681: goto tr838;
	case 682: goto tr838;
	case 683: goto tr838;
	case 684: goto tr838;
	case 685: goto tr838;
	case 686: goto tr838;
	case 1764: goto tr2245;
	case 687: goto tr838;
	case 688: goto tr838;
	case 689: goto tr838;
	case 690: goto tr838;
	case 691: goto tr838;
	case 692: goto tr838;
	case 693: goto tr838;
	case 694: goto tr838;
	case 695: goto tr838;
	case 696: goto tr838;
	case 697: goto tr838;
	case 698: goto tr234;
	case 699: goto tr234;
	case 1765: goto tr2245;
	case 700: goto tr234;
	case 701: goto tr234;
	case 702: goto tr234;
	case 703: goto tr234;
	case 704: goto tr234;
	case 705: goto tr234;
	case 706: goto tr234;
	case 707: goto tr234;
	case 708: goto tr234;
	case 709: goto tr234;
	case 1766: goto tr2245;
	case 1767: goto tr2103;
	case 1768: goto tr2103;
	case 1769: goto tr2103;
	case 1770: goto tr2103;
	case 1771: goto tr2103;
	case 1772: goto tr2103;
	case 1773: goto tr2103;
	case 1774: goto tr2103;
	case 1775: goto tr2103;
	case 1776: goto tr2103;
	case 1777: goto tr2103;
	case 1778: goto tr2103;
	case 710: goto tr237;
	case 711: goto tr237;
	case 1779: goto tr2258;
	case 1780: goto tr2103;
	case 1781: goto tr2103;
	case 1782: goto tr2103;
	case 1783: goto tr2103;
	case 712: goto tr237;
	case 713: goto tr237;
	case 1784: goto tr2264;
	case 1785: goto tr2103;
	case 1786: goto tr2103;
	case 1787: goto tr2103;
	case 714: goto tr237;
	case 715: goto tr237;
	case 716: goto tr237;
	case 717: goto tr237;
	case 718: goto tr237;
	case 719: goto tr237;
	case 720: goto tr237;
	case 721: goto tr237;
	case 722: goto tr237;
	case 1788: goto tr2269;
	case 1789: goto tr2103;
	case 1790: goto tr2103;
	case 1791: goto tr2103;
	case 1792: goto tr2103;
	case 723: goto tr237;
	case 724: goto tr237;
	case 1793: goto tr2275;
	case 1794: goto tr2103;
	case 1795: goto tr2103;
	case 1796: goto tr2103;
	case 1797: goto tr2103;
	case 725: goto tr237;
	case 726: goto tr237;
	case 1798: goto tr2283;
	case 1799: goto tr2103;
	case 1800: goto tr2103;
	case 727: goto tr237;
	case 728: goto tr237;
	case 1801: goto tr2287;
	case 729: goto tr237;
	case 730: goto tr237;
	case 731: goto tr237;
	case 732: goto tr237;
	case 733: goto tr237;
	case 734: goto tr237;
	case 735: goto tr237;
	case 736: goto tr237;
	case 737: goto tr237;
	case 1802: goto tr2289;
	case 1803: goto tr2103;
	case 1804: goto tr2103;
	case 1805: goto tr2103;
	case 738: goto tr237;
	case 739: goto tr237;
	case 1806: goto tr2294;
	case 1807: goto tr2103;
	case 1808: goto tr2103;
	case 1809: goto tr2103;
	case 1810: goto tr2103;
	case 1811: goto tr2103;
	case 1812: goto tr2103;
	case 740: goto tr237;
	case 741: goto tr237;
	case 1813: goto tr2302;
	case 1814: goto tr2103;
	case 1815: goto tr2103;
	case 1816: goto tr2103;
	case 742: goto tr237;
	case 743: goto tr237;
	case 1817: goto tr2307;
	case 1818: goto tr2103;
	case 1819: goto tr2103;
	case 1820: goto tr2103;
	case 1821: goto tr2103;
	case 744: goto tr237;
	case 745: goto tr237;
	case 746: goto tr237;
	case 747: goto tr237;
	case 748: goto tr237;
	case 749: goto tr237;
	case 750: goto tr237;
	case 1822: goto tr2317;
	case 751: goto tr237;
	case 752: goto tr237;
	case 753: goto tr237;
	case 754: goto tr237;
	case 755: goto tr237;
	case 756: goto tr237;
	case 757: goto tr237;
	case 758: goto tr237;
	case 1823: goto tr2103;
	case 1824: goto tr2103;
	case 1825: goto tr2103;
	case 1826: goto tr2103;
	case 1827: goto tr2103;
	case 1828: goto tr2103;
	case 1829: goto tr2103;
	case 1830: goto tr2103;
	case 759: goto tr237;
	case 760: goto tr237;
	case 1831: goto tr2326;
	case 1832: goto tr2103;
	case 1833: goto tr2103;
	case 1834: goto tr2103;
	case 1835: goto tr2103;
	case 1836: goto tr2103;
	case 761: goto tr237;
	case 762: goto tr237;
	case 1837: goto tr2333;
	case 1838: goto tr2103;
	case 1839: goto tr2103;
	case 1840: goto tr2103;
	case 1841: goto tr2103;
	case 1842: goto tr2103;
	case 1843: goto tr2103;
	case 1844: goto tr2103;
	case 1845: goto tr2103;
	case 763: goto tr237;
	case 764: goto tr237;
	case 1846: goto tr2342;
	case 1847: goto tr2103;
	case 1848: goto tr2103;
	case 1849: goto tr2103;
	case 1850: goto tr2103;
	case 765: goto tr237;
	case 766: goto tr237;
	case 767: goto tr237;
	case 1851: goto tr2350;
	case 768: goto tr237;
	case 769: goto tr237;
	case 770: goto tr237;
	case 771: goto tr237;
	case 772: goto tr237;
	case 773: goto tr237;
	case 774: goto tr237;
	case 775: goto tr237;
	case 776: goto tr237;
	case 1852: goto tr2352;
	case 777: goto tr237;
	case 778: goto tr237;
	case 779: goto tr237;
	case 780: goto tr237;
	case 781: goto tr237;
	case 782: goto tr237;
	case 1853: goto tr2350;
	case 1854: goto tr2103;
	case 1855: goto tr2103;
	case 1856: goto tr2103;
	case 1857: goto tr2103;
	case 1858: goto tr2103;
	case 1859: goto tr2103;
	case 1860: goto tr2103;
	case 1861: goto tr2103;
	case 1862: goto tr2103;
	case 1863: goto tr2103;
	case 1864: goto tr2103;
	case 1865: goto tr2102;
	case 783: goto tr241;
	case 784: goto tr241;
	case 785: goto tr234;
	case 786: goto tr234;
	case 787: goto tr234;
	case 788: goto tr234;
	case 789: goto tr234;
	case 790: goto tr234;
	case 791: goto tr234;
	case 792: goto tr234;
	case 793: goto tr234;
	case 794: goto tr234;
	case 795: goto tr241;
	case 796: goto tr241;
	case 797: goto tr234;
	case 798: goto tr234;
	case 799: goto tr234;
	case 800: goto tr234;
	case 801: goto tr234;
	case 802: goto tr234;
	case 803: goto tr234;
	case 804: goto tr234;
	case 805: goto tr234;
	case 806: goto tr234;
	case 1866: goto tr234;
	case 807: goto tr234;
	case 808: goto tr241;
	case 809: goto tr241;
	case 810: goto tr241;
	case 811: goto tr241;
	case 812: goto tr241;
	case 813: goto tr241;
	case 814: goto tr241;
	case 815: goto tr241;
	case 816: goto tr241;
	case 817: goto tr241;
	case 818: goto tr241;
	case 819: goto tr241;
	case 820: goto tr241;
	case 1867: goto tr2378;
	case 821: goto tr241;
	case 822: goto tr241;
	case 823: goto tr241;
	case 824: goto tr241;
	case 825: goto tr241;
	case 826: goto tr241;
	case 827: goto tr241;
	case 828: goto tr241;
	case 829: goto tr241;
	case 830: goto tr241;
	case 831: goto tr241;
	case 832: goto tr241;
	case 833: goto tr241;
	case 834: goto tr241;
	case 835: goto tr241;
	case 836: goto tr241;
	case 837: goto tr241;
	case 838: goto tr241;
	case 839: goto tr241;
	case 840: goto tr241;
	case 841: goto tr241;
	case 842: goto tr241;
	case 843: goto tr241;
	case 844: goto tr241;
	case 845: goto tr241;
	case 846: goto tr241;
	case 847: goto tr241;
	case 848: goto tr241;
	case 849: goto tr241;
	case 850: goto tr241;
	case 851: goto tr241;
	case 852: goto tr241;
	case 853: goto tr241;
	case 854: goto tr241;
	case 855: goto tr241;
	case 856: goto tr241;
	case 857: goto tr241;
	case 858: goto tr241;
	case 859: goto tr241;
	case 860: goto tr241;
	case 861: goto tr241;
	case 862: goto tr241;
	case 863: goto tr241;
	case 864: goto tr241;
	case 865: goto tr241;
	case 866: goto tr241;
	case 867: goto tr241;
	case 868: goto tr241;
	case 869: goto tr241;
	case 1868: goto tr2379;
	case 870: goto tr1051;
	case 1869: goto tr2380;
	case 871: goto tr1054;
	case 872: goto tr241;
	case 873: goto tr241;
	case 874: goto tr241;
	case 875: goto tr241;
	case 876: goto tr241;
	case 877: goto tr241;
	case 878: goto tr241;
	case 879: goto tr241;
	case 880: goto tr241;
	case 881: goto tr241;
	case 882: goto tr241;
	case 883: goto tr241;
	case 884: goto tr241;
	case 885: goto tr241;
	case 886: goto tr241;
	case 887: goto tr241;
	case 1870: goto tr234;
	case 888: goto tr241;
	case 889: goto tr241;
	case 890: goto tr241;
	case 891: goto tr241;
	case 892: goto tr241;
	case 893: goto tr241;
	case 894: goto tr241;
	case 895: goto tr241;
	case 896: goto tr241;
	case 897: goto tr241;
	case 898: goto tr241;
	case 899: goto tr241;
	case 900: goto tr241;
	case 901: goto tr241;
	case 902: goto tr241;
	case 903: goto tr241;
	case 904: goto tr241;
	case 905: goto tr241;
	case 906: goto tr241;
	case 907: goto tr241;
	case 908: goto tr241;
	case 909: goto tr241;
	case 910: goto tr241;
	case 911: goto tr241;
	case 912: goto tr241;
	case 913: goto tr241;
	case 914: goto tr241;
	case 915: goto tr241;
	case 916: goto tr241;
	case 917: goto tr241;
	case 918: goto tr241;
	case 919: goto tr241;
	case 920: goto tr241;
	case 921: goto tr241;
	case 922: goto tr241;
	case 923: goto tr241;
	case 924: goto tr241;
	case 925: goto tr241;
	case 926: goto tr241;
	case 927: goto tr241;
	case 928: goto tr241;
	case 929: goto tr241;
	case 930: goto tr241;
	case 931: goto tr241;
	case 932: goto tr241;
	case 933: goto tr241;
	case 934: goto tr241;
	case 935: goto tr241;
	case 936: goto tr241;
	case 937: goto tr241;
	case 938: goto tr241;
	case 939: goto tr241;
	case 940: goto tr241;
	case 941: goto tr241;
	case 942: goto tr241;
	case 943: goto tr241;
	case 944: goto tr241;
	case 945: goto tr241;
	case 946: goto tr241;
	case 947: goto tr241;
	case 948: goto tr241;
	case 949: goto tr241;
	case 950: goto tr241;
	case 951: goto tr241;
	case 952: goto tr241;
	case 953: goto tr241;
	case 954: goto tr241;
	case 955: goto tr241;
	case 956: goto tr241;
	case 957: goto tr241;
	case 958: goto tr241;
	case 959: goto tr241;
	case 960: goto tr241;
	case 961: goto tr241;
	case 962: goto tr241;
	case 963: goto tr241;
	case 964: goto tr241;
	case 965: goto tr241;
	case 966: goto tr241;
	case 967: goto tr241;
	case 968: goto tr241;
	case 969: goto tr241;
	case 970: goto tr241;
	case 971: goto tr241;
	case 972: goto tr241;
	case 973: goto tr241;
	case 974: goto tr241;
	case 975: goto tr241;
	case 976: goto tr241;
	case 977: goto tr241;
	case 978: goto tr241;
	case 979: goto tr241;
	case 980: goto tr241;
	case 981: goto tr241;
	case 982: goto tr241;
	case 983: goto tr241;
	case 984: goto tr241;
	case 985: goto tr241;
	case 986: goto tr241;
	case 987: goto tr241;
	case 988: goto tr241;
	case 989: goto tr241;
	case 990: goto tr241;
	case 991: goto tr241;
	case 992: goto tr241;
	case 993: goto tr241;
	case 994: goto tr241;
	case 995: goto tr241;
	case 996: goto tr241;
	case 997: goto tr241;
	case 998: goto tr241;
	case 999: goto tr241;
	case 1000: goto tr241;
	case 1001: goto tr241;
	case 1002: goto tr241;
	case 1003: goto tr241;
	case 1004: goto tr241;
	case 1005: goto tr241;
	case 1006: goto tr241;
	case 1007: goto tr241;
	case 1008: goto tr241;
	case 1009: goto tr241;
	case 1010: goto tr241;
	case 1011: goto tr241;
	case 1012: goto tr241;
	case 1013: goto tr241;
	case 1014: goto tr241;
	case 1015: goto tr241;
	case 1016: goto tr241;
	case 1017: goto tr241;
	case 1018: goto tr241;
	case 1019: goto tr241;
	case 1020: goto tr241;
	case 1021: goto tr241;
	case 1022: goto tr241;
	case 1023: goto tr241;
	case 1024: goto tr241;
	case 1025: goto tr241;
	case 1026: goto tr241;
	case 1027: goto tr241;
	case 1028: goto tr241;
	case 1029: goto tr241;
	case 1030: goto tr241;
	case 1031: goto tr241;
	case 1032: goto tr241;
	case 1033: goto tr241;
	case 1034: goto tr241;
	case 1035: goto tr241;
	case 1036: goto tr241;
	case 1037: goto tr241;
	case 1038: goto tr241;
	case 1039: goto tr241;
	case 1040: goto tr241;
	case 1041: goto tr241;
	case 1042: goto tr241;
	case 1043: goto tr234;
	case 1044: goto tr234;
	case 1045: goto tr234;
	case 1046: goto tr241;
	case 1047: goto tr241;
	case 1048: goto tr241;
	case 1871: goto tr2381;
	case 1049: goto tr241;
	case 1050: goto tr241;
	case 1872: goto tr2381;
	case 1051: goto tr241;
	case 1052: goto tr241;
	case 1053: goto tr241;
	case 1054: goto tr241;
	case 1055: goto tr241;
	case 1056: goto tr241;
	case 1057: goto tr241;
	case 1058: goto tr241;
	case 1059: goto tr241;
	case 1060: goto tr241;
	case 1061: goto tr241;
	case 1062: goto tr241;
	case 1063: goto tr241;
	case 1064: goto tr241;
	case 1065: goto tr241;
	case 1066: goto tr241;
	case 1067: goto tr241;
	case 1873: goto tr2382;
	case 1068: goto tr1257;
	case 1069: goto tr241;
	case 1070: goto tr241;
	case 1071: goto tr241;
	case 1072: goto tr241;
	case 1073: goto tr241;
	case 1074: goto tr241;
	case 1075: goto tr241;
	case 1076: goto tr241;
	case 1077: goto tr241;
	case 1078: goto tr241;
	case 1079: goto tr241;
	case 1080: goto tr241;
	case 1081: goto tr241;
	case 1082: goto tr241;
	case 1083: goto tr241;
	case 1084: goto tr241;
	case 1085: goto tr241;
	case 1086: goto tr241;
	case 1087: goto tr241;
	case 1088: goto tr241;
	case 1089: goto tr241;
	case 1090: goto tr241;
	case 1091: goto tr241;
	case 1092: goto tr241;
	case 1093: goto tr241;
	case 1094: goto tr241;
	case 1095: goto tr241;
	case 1096: goto tr241;
	case 1097: goto tr241;
	case 1098: goto tr241;
	case 1099: goto tr241;
	case 1100: goto tr241;
	case 1101: goto tr234;
	case 1102: goto tr234;
	case 1103: goto tr234;
	case 1104: goto tr234;
	case 1105: goto tr234;
	case 1106: goto tr234;
	case 1107: goto tr234;
	case 1108: goto tr234;
	case 1109: goto tr241;
	case 1110: goto tr241;
	case 1874: goto tr2381;
	case 1111: goto tr241;
	case 1112: goto tr241;
	case 1113: goto tr241;
	case 1114: goto tr241;
	case 1115: goto tr241;
	case 1116: goto tr241;
	case 1117: goto tr241;
	case 1118: goto tr241;
	case 1119: goto tr241;
	case 1120: goto tr241;
	case 1121: goto tr241;
	case 1122: goto tr241;
	case 1123: goto tr241;
	case 1124: goto tr241;
	case 1125: goto tr234;
	case 1126: goto tr234;
	case 1127: goto tr241;
	case 1128: goto tr241;
	case 1129: goto tr241;
	case 1130: goto tr241;
	case 1875: goto tr2381;
	case 1131: goto tr241;
	case 1132: goto tr241;
	case 1133: goto tr241;
	case 1134: goto tr241;
	case 1135: goto tr241;
	case 1136: goto tr241;
	case 1137: goto tr241;
	case 1138: goto tr241;
	case 1139: goto tr241;
	case 1140: goto tr241;
	case 1141: goto tr241;
	case 1142: goto tr241;
	case 1143: goto tr241;
	case 1144: goto tr241;
	case 1145: goto tr241;
	case 1146: goto tr241;
	case 1147: goto tr241;
	case 1148: goto tr241;
	case 1149: goto tr241;
	case 1150: goto tr241;
	case 1151: goto tr234;
	case 1152: goto tr241;
	case 1153: goto tr241;
	case 1876: goto tr2381;
	case 1154: goto tr241;
	case 1155: goto tr241;
	case 1156: goto tr241;
	case 1157: goto tr241;
	case 1158: goto tr241;
	case 1159: goto tr241;
	case 1160: goto tr241;
	case 1161: goto tr241;
	case 1162: goto tr241;
	case 1163: goto tr241;
	case 1164: goto tr241;
	case 1165: goto tr241;
	case 1166: goto tr241;
	case 1167: goto tr241;
	case 1168: goto tr241;
	case 1169: goto tr241;
	case 1170: goto tr241;
	case 1171: goto tr241;
	case 1172: goto tr241;
	case 1173: goto tr241;
	case 1174: goto tr241;
	case 1175: goto tr241;
	case 1176: goto tr241;
	case 1177: goto tr241;
	case 1178: goto tr234;
	case 1179: goto tr241;
	case 1180: goto tr241;
	case 1877: goto tr2381;
	case 1181: goto tr241;
	case 1182: goto tr241;
	case 1183: goto tr241;
	case 1184: goto tr241;
	case 1185: goto tr241;
	case 1186: goto tr241;
	case 1187: goto tr241;
	case 1188: goto tr241;
	case 1189: goto tr241;
	case 1190: goto tr241;
	case 1191: goto tr241;
	case 1192: goto tr241;
	case 1193: goto tr241;
	case 1194: goto tr241;
	case 1195: goto tr241;
	case 1196: goto tr241;
	case 1197: goto tr241;
	case 1198: goto tr241;
	case 1199: goto tr241;
	case 1200: goto tr241;
	case 1201: goto tr241;
	case 1202: goto tr241;
	case 1203: goto tr241;
	case 1204: goto tr241;
	case 1205: goto tr241;
	case 1206: goto tr241;
	case 1207: goto tr241;
	case 1208: goto tr241;
	case 1209: goto tr241;
	case 1210: goto tr241;
	case 1211: goto tr241;
	case 1212: goto tr241;
	case 1213: goto tr241;
	case 1214: goto tr241;
	case 1215: goto tr241;
	case 1216: goto tr241;
	case 1217: goto tr241;
	case 1218: goto tr241;
	case 1219: goto tr241;
	case 1220: goto tr241;
	case 1221: goto tr241;
	case 1222: goto tr241;
	case 1223: goto tr241;
	case 1224: goto tr241;
	case 1225: goto tr241;
	case 1226: goto tr241;
	case 1227: goto tr241;
	case 1228: goto tr241;
	case 1229: goto tr241;
	case 1230: goto tr241;
	case 1231: goto tr241;
	case 1232: goto tr241;
	case 1233: goto tr241;
	case 1234: goto tr241;
	case 1235: goto tr241;
	case 1236: goto tr241;
	case 1237: goto tr241;
	case 1238: goto tr241;
	case 1239: goto tr241;
	case 1240: goto tr241;
	case 1241: goto tr241;
	case 1242: goto tr241;
	case 1243: goto tr241;
	case 1244: goto tr241;
	case 1245: goto tr241;
	case 1246: goto tr241;
	case 1247: goto tr241;
	case 1248: goto tr241;
	case 1249: goto tr241;
	case 1250: goto tr241;
	case 1251: goto tr241;
	case 1252: goto tr241;
	case 1253: goto tr241;
	case 1254: goto tr241;
	case 1255: goto tr241;
	case 1256: goto tr241;
	case 1257: goto tr241;
	case 1258: goto tr241;
	case 1259: goto tr241;
	case 1260: goto tr241;
	case 1261: goto tr241;
	case 1262: goto tr241;
	case 1878: goto tr2102;
	case 1879: goto tr2102;
	case 1263: goto tr241;
	case 1264: goto tr241;
	case 1265: goto tr241;
	case 1266: goto tr241;
	case 1267: goto tr241;
	case 1268: goto tr241;
	case 1269: goto tr241;
	case 1270: goto tr241;
	case 1271: goto tr241;
	case 1272: goto tr241;
	case 1273: goto tr241;
	case 1274: goto tr241;
	case 1275: goto tr241;
	case 1276: goto tr241;
	case 1277: goto tr241;
	case 1278: goto tr241;
	case 1279: goto tr241;
	case 1280: goto tr241;
	case 1281: goto tr241;
	case 1282: goto tr241;
	case 1283: goto tr241;
	case 1284: goto tr241;
	case 1285: goto tr241;
	case 1286: goto tr241;
	case 1287: goto tr241;
	case 1288: goto tr241;
	case 1289: goto tr241;
	case 1290: goto tr241;
	case 1291: goto tr241;
	case 1292: goto tr241;
	case 1293: goto tr241;
	case 1294: goto tr241;
	case 1295: goto tr241;
	case 1296: goto tr241;
	case 1297: goto tr241;
	case 1298: goto tr241;
	case 1299: goto tr241;
	case 1300: goto tr241;
	case 1301: goto tr241;
	case 1302: goto tr241;
	case 1303: goto tr241;
	case 1304: goto tr241;
	case 1305: goto tr241;
	case 1880: goto tr2379;
	case 1306: goto tr241;
	case 1307: goto tr241;
	case 1308: goto tr241;
	case 1309: goto tr241;
	case 1310: goto tr241;
	case 1311: goto tr241;
	case 1312: goto tr241;
	case 1313: goto tr241;
	case 1314: goto tr241;
	case 1315: goto tr241;
	case 1316: goto tr241;
	case 1317: goto tr241;
	case 1318: goto tr241;
	case 1319: goto tr241;
	case 1320: goto tr241;
	case 1321: goto tr241;
	case 1322: goto tr241;
	case 1323: goto tr241;
	case 1324: goto tr241;
	case 1325: goto tr241;
	case 1326: goto tr241;
	case 1327: goto tr241;
	case 1328: goto tr241;
	case 1329: goto tr241;
	case 1330: goto tr241;
	case 1331: goto tr241;
	case 1332: goto tr241;
	case 1333: goto tr241;
	case 1334: goto tr241;
	case 1335: goto tr241;
	case 1336: goto tr241;
	case 1337: goto tr241;
	case 1338: goto tr241;
	case 1339: goto tr241;
	case 1881: goto tr2102;
	case 1340: goto tr241;
	case 1341: goto tr241;
	case 1882: goto tr2102;
	case 1342: goto tr241;
	case 1343: goto tr234;
	case 1344: goto tr234;
	case 1345: goto tr234;
	case 1883: goto tr2402;
	case 1346: goto tr234;
	case 1347: goto tr234;
	case 1348: goto tr234;
	case 1349: goto tr234;
	case 1350: goto tr234;
	case 1351: goto tr234;
	case 1352: goto tr234;
	case 1353: goto tr234;
	case 1354: goto tr234;
	case 1355: goto tr234;
	case 1884: goto tr2402;
	case 1356: goto tr234;
	case 1357: goto tr234;
	case 1358: goto tr234;
	case 1359: goto tr234;
	case 1360: goto tr234;
	case 1361: goto tr234;
	case 1362: goto tr234;
	case 1363: goto tr234;
	case 1364: goto tr234;
	case 1365: goto tr234;
	case 1366: goto tr241;
	case 1367: goto tr241;
	case 1368: goto tr241;
	case 1369: goto tr241;
	case 1370: goto tr241;
	case 1371: goto tr241;
	case 1372: goto tr241;
	case 1373: goto tr241;
	case 1374: goto tr241;
	case 1375: goto tr241;
	case 1886: goto tr2408;
	case 1376: goto tr1628;
	case 1377: goto tr1628;
	case 1378: goto tr1628;
	case 1379: goto tr1628;
	case 1380: goto tr1628;
	case 1381: goto tr1628;
	case 1382: goto tr1628;
	case 1383: goto tr1628;
	case 1384: goto tr1628;
	case 1385: goto tr1628;
	case 1386: goto tr1628;
	case 1387: goto tr1628;
	case 1887: goto tr2408;
	case 1888: goto tr2408;
	case 1890: goto tr2416;
	case 1388: goto tr1640;
	case 1389: goto tr1640;
	case 1390: goto tr1640;
	case 1391: goto tr1640;
	case 1392: goto tr1640;
	case 1393: goto tr1640;
	case 1394: goto tr1640;
	case 1395: goto tr1640;
	case 1396: goto tr1640;
	case 1397: goto tr1640;
	case 1398: goto tr1640;
	case 1399: goto tr1640;
	case 1400: goto tr1640;
	case 1401: goto tr1640;
	case 1402: goto tr1640;
	case 1403: goto tr1640;
	case 1404: goto tr1640;
	case 1405: goto tr1640;
	case 1891: goto tr2416;
	case 1892: goto tr2416;
	case 1894: goto tr2422;
	case 1406: goto tr1658;
	case 1407: goto tr1658;
	case 1408: goto tr1658;
	case 1409: goto tr1658;
	case 1410: goto tr1658;
	case 1411: goto tr1658;
	case 1412: goto tr1658;
	case 1413: goto tr1658;
	case 1414: goto tr1658;
	case 1415: goto tr1658;
	case 1416: goto tr1658;
	case 1417: goto tr1658;
	case 1418: goto tr1658;
	case 1419: goto tr1658;
	case 1420: goto tr1658;
	case 1421: goto tr1658;
	case 1422: goto tr1658;
	case 1423: goto tr1658;
	case 1424: goto tr1658;
	case 1425: goto tr1658;
	case 1426: goto tr1658;
	case 1427: goto tr1658;
	case 1428: goto tr1658;
	case 1429: goto tr1658;
	case 1430: goto tr1658;
	case 1431: goto tr1658;
	case 1432: goto tr1658;
	case 1433: goto tr1658;
	case 1434: goto tr1658;
	case 1435: goto tr1658;
	case 1436: goto tr1658;
	case 1437: goto tr1658;
	case 1438: goto tr1658;
	case 1439: goto tr1658;
	case 1440: goto tr1658;
	case 1441: goto tr1658;
	case 1442: goto tr1658;
	case 1443: goto tr1658;
	case 1444: goto tr1658;
	case 1445: goto tr1658;
	case 1446: goto tr1658;
	case 1447: goto tr1658;
	case 1448: goto tr1658;
	case 1449: goto tr1658;
	case 1450: goto tr1658;
	case 1451: goto tr1658;
	case 1452: goto tr1658;
	case 1453: goto tr1658;
	case 1454: goto tr1658;
	case 1455: goto tr1658;
	case 1456: goto tr1658;
	case 1457: goto tr1658;
	case 1458: goto tr1658;
	case 1459: goto tr1658;
	case 1460: goto tr1658;
	case 1461: goto tr1658;
	case 1462: goto tr1658;
	case 1463: goto tr1658;
	case 1464: goto tr1658;
	case 1465: goto tr1658;
	case 1466: goto tr1658;
	case 1467: goto tr1658;
	case 1468: goto tr1658;
	case 1469: goto tr1658;
	case 1470: goto tr1658;
	case 1471: goto tr1658;
	case 1472: goto tr1658;
	case 1473: goto tr1658;
	case 1474: goto tr1658;
	case 1475: goto tr1658;
	case 1476: goto tr1658;
	case 1477: goto tr1658;
	case 1478: goto tr1658;
	case 1479: goto tr1658;
	case 1480: goto tr1658;
	case 1481: goto tr1658;
	case 1482: goto tr1658;
	case 1483: goto tr1658;
	case 1484: goto tr1658;
	case 1485: goto tr1658;
	case 1486: goto tr1658;
	case 1487: goto tr1658;
	case 1488: goto tr1658;
	case 1489: goto tr1658;
	case 1490: goto tr1658;
	case 1491: goto tr1658;
	case 1492: goto tr1658;
	case 1493: goto tr1658;
	case 1494: goto tr1658;
	case 1495: goto tr1658;
	case 1496: goto tr1658;
	case 1497: goto tr1658;
	case 1498: goto tr1658;
	case 1499: goto tr1658;
	case 1500: goto tr1658;
	case 1501: goto tr1658;
	case 1502: goto tr1658;
	case 1503: goto tr1658;
	case 1504: goto tr1658;
	case 1505: goto tr1658;
	case 1506: goto tr1658;
	case 1507: goto tr1658;
	case 1508: goto tr1658;
	case 1509: goto tr1658;
	case 1510: goto tr1658;
	case 1511: goto tr1658;
	case 1512: goto tr1658;
	case 1513: goto tr1658;
	case 1514: goto tr1658;
	case 1515: goto tr1658;
	case 1516: goto tr1658;
	case 1517: goto tr1658;
	case 1895: goto tr2422;
	case 1518: goto tr1658;
	case 1519: goto tr1658;
	case 1520: goto tr1658;
	case 1521: goto tr1658;
	case 1522: goto tr1658;
	case 1523: goto tr1658;
	case 1524: goto tr1658;
	case 1525: goto tr1658;
	case 1526: goto tr1658;
	case 1527: goto tr1658;
	case 1528: goto tr1658;
	case 1529: goto tr1658;
	case 1530: goto tr1658;
	case 1531: goto tr1658;
	case 1532: goto tr1658;
	case 1533: goto tr1658;
	case 1534: goto tr1658;
	case 1535: goto tr1658;
	case 1536: goto tr1658;
	case 1537: goto tr1658;
	case 1538: goto tr1658;
	case 1539: goto tr1658;
	case 1540: goto tr1658;
	case 1541: goto tr1658;
	case 1542: goto tr1658;
	case 1543: goto tr1658;
	case 1544: goto tr1658;
	case 1545: goto tr1658;
	case 1546: goto tr1658;
	case 1547: goto tr1658;
	case 1548: goto tr1658;
	case 1549: goto tr1658;
	case 1550: goto tr1658;
	case 1551: goto tr1658;
	case 1552: goto tr1658;
	case 1553: goto tr1658;
	case 1554: goto tr1658;
	case 1555: goto tr1658;
	case 1556: goto tr1658;
	case 1557: goto tr1658;
	case 1558: goto tr1658;
	case 1559: goto tr1658;
	case 1560: goto tr1658;
	case 1561: goto tr1658;
	case 1562: goto tr1658;
	case 1563: goto tr1658;
	case 1564: goto tr1658;
	case 1565: goto tr1658;
	case 1566: goto tr1658;
	case 1567: goto tr1658;
	case 1568: goto tr1658;
	case 1569: goto tr1658;
	case 1570: goto tr1658;
	case 1571: goto tr1658;
	case 1572: goto tr1658;
	case 1573: goto tr1658;
	case 1574: goto tr1658;
	case 1575: goto tr1658;
	case 1576: goto tr1658;
	case 1577: goto tr1658;
	case 1578: goto tr1658;
	case 1579: goto tr1658;
	case 1580: goto tr1658;
	case 1581: goto tr1658;
	case 1582: goto tr1658;
	case 1583: goto tr1658;
	case 1584: goto tr1658;
	case 1585: goto tr1658;
	case 1586: goto tr1658;
	case 1587: goto tr1658;
	case 1588: goto tr1658;
	case 1589: goto tr1658;
	case 1590: goto tr1658;
	case 1591: goto tr1658;
	case 1592: goto tr1658;
	case 1593: goto tr1658;
	case 1594: goto tr1658;
	case 1595: goto tr1658;
	case 1596: goto tr1658;
	case 1597: goto tr1658;
	case 1598: goto tr1658;
	case 1599: goto tr1658;
	case 1600: goto tr1658;
	case 1601: goto tr1658;
	case 1602: goto tr1658;
	case 1603: goto tr1658;
	case 1604: goto tr1658;
	case 1605: goto tr1658;
	case 1606: goto tr1658;
	case 1607: goto tr1658;
	case 1608: goto tr1658;
	case 1609: goto tr1658;
	case 1610: goto tr1658;
	case 1611: goto tr1658;
	case 1612: goto tr1658;
	case 1613: goto tr1658;
	case 1614: goto tr1658;
	case 1615: goto tr1658;
	case 1616: goto tr1658;
	case 1617: goto tr1658;
	case 1618: goto tr1658;
	case 1619: goto tr1658;
	case 1620: goto tr1658;
	case 1621: goto tr1658;
	case 1622: goto tr1658;
	case 1623: goto tr1658;
	case 1624: goto tr1658;
	case 1625: goto tr1658;
	case 1626: goto tr1658;
	case 1627: goto tr1658;
	case 1628: goto tr1658;
	case 1629: goto tr1658;
	}
	}

	_out: {}
	}

#line 1474 "ext/dtext/dtext.cpp.rl"

  g_debug("EOF; closing stray blocks");
  dstack_close_all();
  g_debug("done");

  return output;
}
