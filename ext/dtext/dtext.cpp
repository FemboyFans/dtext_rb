
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


#line 821 "ext/dtext/dtext.cpp.rl"



#line 54 "ext/dtext/dtext.cpp"
static const int dtext_start = 1679;
static const int dtext_first_final = 1679;
static const int dtext_error = 0;

static const int dtext_en_basic_inline = 1705;
static const int dtext_en_inline = 1708;
static const int dtext_en_code = 1938;
static const int dtext_en_nodtext = 1942;
static const int dtext_en_table = 1946;
static const int dtext_en_main = 1679;


#line 824 "ext/dtext/dtext.cpp.rl"

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
  if (std::all_of(normalized_tag.cbegin(), normalized_tag.cend(), ::isdigit)) {
  append_relative_url("/wiki_pages/");
  } else {
  append_relative_url("/wiki_pages/show_or_new?title=");
  }
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

void StateMachine::append_internal_anchor_link(const std::string_view prefix, const std::string_view anchor, const std::string_view title, const std::string_view suffix) {
  auto anchor_string = std::string(anchor);
  auto title_string = std::string(title);

  // 19[[60s]] -> [[60s|1960s]]
  if (!prefix.empty()) {
    title_string.insert(0, prefix);
  }

  // [[cat]]s -> [[cat|cats]]
  if (!suffix.empty()) {
    title_string.append(suffix);
  }

  append("<a class=\"dtext-link dtext-internal-anchor-link\" href=\"#");
  append_uri_escaped(anchor_string);
  append("\">");
  append_html_escaped(title_string);
  append("</a>");

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
    case BLOCK_SECTION: append_block("</div></details>"); break;
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
    case INLINE_SUP: append("</sup>"); break;
    case INLINE_SUB: append("</sub>"); break;
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

// container blocks: [spoiler], [quote], [section], [tn]
// leaf blocks: [nodtext], [code], [table], [td]?, [th]?, <h1>, <p>, <li>, <ul>
void StateMachine::dstack_close_leaf_blocks() {
  g_debug("dstack close leaf blocks");

  while (!dstack.empty() && !dstack_check(BLOCK_QUOTE) && !dstack_check(BLOCK_SPOILER) && !dstack_check(BLOCK_SECTION) && !dstack_check(BLOCK_TN)) {
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

  
#line 774 "ext/dtext/dtext.cpp"
	{
	( top) = 0;
	( ts) = 0;
	( te) = 0;
	( act) = 0;
	}

#line 1530 "ext/dtext/dtext.cpp.rl"
  
#line 784 "ext/dtext/dtext.cpp"
	{
	short _widec;
	if ( ( p) == ( pe) )
		goto _test_eof;
	goto _resume;

_again:
	switch ( ( cs) ) {
		case 1679: goto st1679;
		case 1680: goto st1680;
		case 1: goto st1;
		case 1681: goto st1681;
		case 2: goto st2;
		case 1682: goto st1682;
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
		case 1683: goto st1683;
		case 17: goto st17;
		case 18: goto st18;
		case 19: goto st19;
		case 20: goto st20;
		case 21: goto st21;
		case 22: goto st22;
		case 23: goto st23;
		case 24: goto st24;
		case 25: goto st25;
		case 26: goto st26;
		case 27: goto st27;
		case 28: goto st28;
		case 1684: goto st1684;
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
		case 45: goto st45;
		case 46: goto st46;
		case 1685: goto st1685;
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
		case 60: goto st60;
		case 61: goto st61;
		case 1686: goto st1686;
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
		case 75: goto st75;
		case 76: goto st76;
		case 1687: goto st1687;
		case 1688: goto st1688;
		case 77: goto st77;
		case 1689: goto st1689;
		case 1690: goto st1690;
		case 78: goto st78;
		case 0: goto st0;
		case 1691: goto st1691;
		case 79: goto st79;
		case 80: goto st80;
		case 81: goto st81;
		case 1692: goto st1692;
		case 1693: goto st1693;
		case 82: goto st82;
		case 83: goto st83;
		case 84: goto st84;
		case 85: goto st85;
		case 86: goto st86;
		case 87: goto st87;
		case 88: goto st88;
		case 89: goto st89;
		case 90: goto st90;
		case 91: goto st91;
		case 1694: goto st1694;
		case 92: goto st92;
		case 93: goto st93;
		case 94: goto st94;
		case 95: goto st95;
		case 96: goto st96;
		case 97: goto st97;
		case 98: goto st98;
		case 99: goto st99;
		case 100: goto st100;
		case 101: goto st101;
		case 1695: goto st1695;
		case 102: goto st102;
		case 103: goto st103;
		case 104: goto st104;
		case 105: goto st105;
		case 106: goto st106;
		case 107: goto st107;
		case 108: goto st108;
		case 1696: goto st1696;
		case 109: goto st109;
		case 1697: goto st1697;
		case 110: goto st110;
		case 111: goto st111;
		case 112: goto st112;
		case 113: goto st113;
		case 114: goto st114;
		case 115: goto st115;
		case 116: goto st116;
		case 117: goto st117;
		case 1698: goto st1698;
		case 118: goto st118;
		case 119: goto st119;
		case 120: goto st120;
		case 121: goto st121;
		case 122: goto st122;
		case 123: goto st123;
		case 124: goto st124;
		case 125: goto st125;
		case 126: goto st126;
		case 127: goto st127;
		case 1699: goto st1699;
		case 128: goto st128;
		case 1700: goto st1700;
		case 129: goto st129;
		case 130: goto st130;
		case 131: goto st131;
		case 132: goto st132;
		case 133: goto st133;
		case 134: goto st134;
		case 1701: goto st1701;
		case 135: goto st135;
		case 136: goto st136;
		case 137: goto st137;
		case 1702: goto st1702;
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
		case 174: goto st174;
		case 175: goto st175;
		case 1703: goto st1703;
		case 1704: goto st1704;
		case 1705: goto st1705;
		case 1706: goto st1706;
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
		case 193: goto st193;
		case 194: goto st194;
		case 195: goto st195;
		case 196: goto st196;
		case 197: goto st197;
		case 198: goto st198;
		case 199: goto st199;
		case 200: goto st200;
		case 1707: goto st1707;
		case 201: goto st201;
		case 202: goto st202;
		case 203: goto st203;
		case 204: goto st204;
		case 205: goto st205;
		case 206: goto st206;
		case 207: goto st207;
		case 208: goto st208;
		case 209: goto st209;
		case 210: goto st210;
		case 211: goto st211;
		case 212: goto st212;
		case 213: goto st213;
		case 214: goto st214;
		case 215: goto st215;
		case 1708: goto st1708;
		case 1709: goto st1709;
		case 1710: goto st1710;
		case 216: goto st216;
		case 217: goto st217;
		case 218: goto st218;
		case 1711: goto st1711;
		case 1712: goto st1712;
		case 1713: goto st1713;
		case 219: goto st219;
		case 1714: goto st1714;
		case 220: goto st220;
		case 1715: goto st1715;
		case 221: goto st221;
		case 222: goto st222;
		case 223: goto st223;
		case 224: goto st224;
		case 225: goto st225;
		case 226: goto st226;
		case 227: goto st227;
		case 228: goto st228;
		case 229: goto st229;
		case 230: goto st230;
		case 231: goto st231;
		case 232: goto st232;
		case 233: goto st233;
		case 1716: goto st1716;
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
		case 1717: goto st1717;
		case 246: goto st246;
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
		case 1718: goto st1718;
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
		case 292: goto st292;
		case 293: goto st293;
		case 294: goto st294;
		case 295: goto st295;
		case 296: goto st296;
		case 297: goto st297;
		case 1719: goto st1719;
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
		case 1720: goto st1720;
		case 1721: goto st1721;
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
		case 1722: goto st1722;
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
		case 383: goto st383;
		case 384: goto st384;
		case 385: goto st385;
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
		case 397: goto st397;
		case 398: goto st398;
		case 399: goto st399;
		case 400: goto st400;
		case 1723: goto st1723;
		case 401: goto st401;
		case 402: goto st402;
		case 403: goto st403;
		case 1724: goto st1724;
		case 404: goto st404;
		case 405: goto st405;
		case 406: goto st406;
		case 407: goto st407;
		case 408: goto st408;
		case 409: goto st409;
		case 410: goto st410;
		case 411: goto st411;
		case 412: goto st412;
		case 413: goto st413;
		case 414: goto st414;
		case 1725: goto st1725;
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
		case 1726: goto st1726;
		case 428: goto st428;
		case 429: goto st429;
		case 430: goto st430;
		case 431: goto st431;
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
		case 443: goto st443;
		case 444: goto st444;
		case 445: goto st445;
		case 446: goto st446;
		case 447: goto st447;
		case 448: goto st448;
		case 449: goto st449;
		case 1727: goto st1727;
		case 450: goto st450;
		case 451: goto st451;
		case 452: goto st452;
		case 453: goto st453;
		case 454: goto st454;
		case 455: goto st455;
		case 456: goto st456;
		case 457: goto st457;
		case 458: goto st458;
		case 459: goto st459;
		case 460: goto st460;
		case 1728: goto st1728;
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
		case 1729: goto st1729;
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
		case 501: goto st501;
		case 502: goto st502;
		case 503: goto st503;
		case 504: goto st504;
		case 505: goto st505;
		case 506: goto st506;
		case 507: goto st507;
		case 508: goto st508;
		case 509: goto st509;
		case 510: goto st510;
		case 511: goto st511;
		case 512: goto st512;
		case 513: goto st513;
		case 514: goto st514;
		case 515: goto st515;
		case 516: goto st516;
		case 517: goto st517;
		case 518: goto st518;
		case 1730: goto st1730;
		case 519: goto st519;
		case 520: goto st520;
		case 521: goto st521;
		case 522: goto st522;
		case 523: goto st523;
		case 524: goto st524;
		case 525: goto st525;
		case 526: goto st526;
		case 527: goto st527;
		case 1731: goto st1731;
		case 1732: goto st1732;
		case 528: goto st528;
		case 529: goto st529;
		case 530: goto st530;
		case 531: goto st531;
		case 1733: goto st1733;
		case 1734: goto st1734;
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
		case 1735: goto st1735;
		case 1736: goto st1736;
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
		case 566: goto st566;
		case 567: goto st567;
		case 568: goto st568;
		case 569: goto st569;
		case 570: goto st570;
		case 571: goto st571;
		case 572: goto st572;
		case 573: goto st573;
		case 574: goto st574;
		case 575: goto st575;
		case 1737: goto st1737;
		case 1738: goto st1738;
		case 576: goto st576;
		case 577: goto st577;
		case 578: goto st578;
		case 579: goto st579;
		case 1739: goto st1739;
		case 1740: goto st1740;
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
		case 1741: goto st1741;
		case 1742: goto st1742;
		case 592: goto st592;
		case 1743: goto st1743;
		case 1744: goto st1744;
		case 593: goto st593;
		case 594: goto st594;
		case 595: goto st595;
		case 596: goto st596;
		case 597: goto st597;
		case 598: goto st598;
		case 599: goto st599;
		case 600: goto st600;
		case 1745: goto st1745;
		case 1746: goto st1746;
		case 601: goto st601;
		case 1747: goto st1747;
		case 602: goto st602;
		case 603: goto st603;
		case 604: goto st604;
		case 605: goto st605;
		case 606: goto st606;
		case 607: goto st607;
		case 608: goto st608;
		case 609: goto st609;
		case 610: goto st610;
		case 611: goto st611;
		case 612: goto st612;
		case 613: goto st613;
		case 614: goto st614;
		case 615: goto st615;
		case 616: goto st616;
		case 617: goto st617;
		case 618: goto st618;
		case 619: goto st619;
		case 1748: goto st1748;
		case 620: goto st620;
		case 621: goto st621;
		case 622: goto st622;
		case 623: goto st623;
		case 624: goto st624;
		case 625: goto st625;
		case 626: goto st626;
		case 627: goto st627;
		case 628: goto st628;
		case 1749: goto st1749;
		case 1750: goto st1750;
		case 1751: goto st1751;
		case 1752: goto st1752;
		case 1753: goto st1753;
		case 629: goto st629;
		case 630: goto st630;
		case 1754: goto st1754;
		case 1755: goto st1755;
		case 1756: goto st1756;
		case 1757: goto st1757;
		case 1758: goto st1758;
		case 1759: goto st1759;
		case 631: goto st631;
		case 632: goto st632;
		case 1760: goto st1760;
		case 633: goto st633;
		case 634: goto st634;
		case 635: goto st635;
		case 636: goto st636;
		case 637: goto st637;
		case 638: goto st638;
		case 639: goto st639;
		case 640: goto st640;
		case 641: goto st641;
		case 1761: goto st1761;
		case 1762: goto st1762;
		case 1763: goto st1763;
		case 1764: goto st1764;
		case 1765: goto st1765;
		case 642: goto st642;
		case 643: goto st643;
		case 644: goto st644;
		case 645: goto st645;
		case 646: goto st646;
		case 647: goto st647;
		case 648: goto st648;
		case 649: goto st649;
		case 650: goto st650;
		case 651: goto st651;
		case 1766: goto st1766;
		case 1767: goto st1767;
		case 1768: goto st1768;
		case 1769: goto st1769;
		case 652: goto st652;
		case 653: goto st653;
		case 1770: goto st1770;
		case 1771: goto st1771;
		case 1772: goto st1772;
		case 654: goto st654;
		case 655: goto st655;
		case 1773: goto st1773;
		case 1774: goto st1774;
		case 1775: goto st1775;
		case 1776: goto st1776;
		case 1777: goto st1777;
		case 1778: goto st1778;
		case 1779: goto st1779;
		case 1780: goto st1780;
		case 656: goto st656;
		case 657: goto st657;
		case 1781: goto st1781;
		case 1782: goto st1782;
		case 1783: goto st1783;
		case 658: goto st658;
		case 659: goto st659;
		case 1784: goto st1784;
		case 1785: goto st1785;
		case 1786: goto st1786;
		case 1787: goto st1787;
		case 1788: goto st1788;
		case 1789: goto st1789;
		case 660: goto st660;
		case 661: goto st661;
		case 1790: goto st1790;
		case 662: goto st662;
		case 1791: goto st1791;
		case 1792: goto st1792;
		case 1793: goto st1793;
		case 663: goto st663;
		case 664: goto st664;
		case 1794: goto st1794;
		case 1795: goto st1795;
		case 1796: goto st1796;
		case 1797: goto st1797;
		case 1798: goto st1798;
		case 665: goto st665;
		case 666: goto st666;
		case 1799: goto st1799;
		case 1800: goto st1800;
		case 1801: goto st1801;
		case 1802: goto st1802;
		case 1803: goto st1803;
		case 667: goto st667;
		case 668: goto st668;
		case 669: goto st669;
		case 1804: goto st1804;
		case 670: goto st670;
		case 671: goto st671;
		case 672: goto st672;
		case 673: goto st673;
		case 674: goto st674;
		case 675: goto st675;
		case 676: goto st676;
		case 677: goto st677;
		case 678: goto st678;
		case 679: goto st679;
		case 680: goto st680;
		case 1805: goto st1805;
		case 681: goto st681;
		case 682: goto st682;
		case 1806: goto st1806;
		case 1807: goto st1807;
		case 1808: goto st1808;
		case 1809: goto st1809;
		case 1810: goto st1810;
		case 1811: goto st1811;
		case 1812: goto st1812;
		case 1813: goto st1813;
		case 1814: goto st1814;
		case 683: goto st683;
		case 684: goto st684;
		case 685: goto st685;
		case 686: goto st686;
		case 687: goto st687;
		case 688: goto st688;
		case 689: goto st689;
		case 690: goto st690;
		case 691: goto st691;
		case 1815: goto st1815;
		case 692: goto st692;
		case 693: goto st693;
		case 694: goto st694;
		case 695: goto st695;
		case 696: goto st696;
		case 697: goto st697;
		case 698: goto st698;
		case 699: goto st699;
		case 700: goto st700;
		case 701: goto st701;
		case 1816: goto st1816;
		case 702: goto st702;
		case 703: goto st703;
		case 704: goto st704;
		case 705: goto st705;
		case 706: goto st706;
		case 707: goto st707;
		case 708: goto st708;
		case 709: goto st709;
		case 710: goto st710;
		case 711: goto st711;
		case 712: goto st712;
		case 1817: goto st1817;
		case 713: goto st713;
		case 714: goto st714;
		case 715: goto st715;
		case 716: goto st716;
		case 717: goto st717;
		case 718: goto st718;
		case 719: goto st719;
		case 720: goto st720;
		case 721: goto st721;
		case 722: goto st722;
		case 723: goto st723;
		case 724: goto st724;
		case 725: goto st725;
		case 1818: goto st1818;
		case 726: goto st726;
		case 727: goto st727;
		case 728: goto st728;
		case 729: goto st729;
		case 730: goto st730;
		case 731: goto st731;
		case 732: goto st732;
		case 733: goto st733;
		case 734: goto st734;
		case 735: goto st735;
		case 1819: goto st1819;
		case 1820: goto st1820;
		case 1821: goto st1821;
		case 1822: goto st1822;
		case 1823: goto st1823;
		case 1824: goto st1824;
		case 1825: goto st1825;
		case 1826: goto st1826;
		case 1827: goto st1827;
		case 1828: goto st1828;
		case 1829: goto st1829;
		case 1830: goto st1830;
		case 1831: goto st1831;
		case 736: goto st736;
		case 737: goto st737;
		case 1832: goto st1832;
		case 1833: goto st1833;
		case 1834: goto st1834;
		case 1835: goto st1835;
		case 1836: goto st1836;
		case 738: goto st738;
		case 739: goto st739;
		case 1837: goto st1837;
		case 1838: goto st1838;
		case 1839: goto st1839;
		case 1840: goto st1840;
		case 740: goto st740;
		case 741: goto st741;
		case 742: goto st742;
		case 743: goto st743;
		case 744: goto st744;
		case 745: goto st745;
		case 746: goto st746;
		case 747: goto st747;
		case 748: goto st748;
		case 1841: goto st1841;
		case 1842: goto st1842;
		case 1843: goto st1843;
		case 1844: goto st1844;
		case 1845: goto st1845;
		case 749: goto st749;
		case 750: goto st750;
		case 1846: goto st1846;
		case 1847: goto st1847;
		case 1848: goto st1848;
		case 1849: goto st1849;
		case 1850: goto st1850;
		case 751: goto st751;
		case 752: goto st752;
		case 1851: goto st1851;
		case 1852: goto st1852;
		case 1853: goto st1853;
		case 753: goto st753;
		case 754: goto st754;
		case 1854: goto st1854;
		case 755: goto st755;
		case 756: goto st756;
		case 757: goto st757;
		case 758: goto st758;
		case 759: goto st759;
		case 760: goto st760;
		case 761: goto st761;
		case 762: goto st762;
		case 763: goto st763;
		case 1855: goto st1855;
		case 1856: goto st1856;
		case 1857: goto st1857;
		case 1858: goto st1858;
		case 764: goto st764;
		case 765: goto st765;
		case 1859: goto st1859;
		case 1860: goto st1860;
		case 1861: goto st1861;
		case 1862: goto st1862;
		case 1863: goto st1863;
		case 1864: goto st1864;
		case 1865: goto st1865;
		case 766: goto st766;
		case 767: goto st767;
		case 1866: goto st1866;
		case 1867: goto st1867;
		case 1868: goto st1868;
		case 1869: goto st1869;
		case 768: goto st768;
		case 769: goto st769;
		case 1870: goto st1870;
		case 1871: goto st1871;
		case 1872: goto st1872;
		case 1873: goto st1873;
		case 1874: goto st1874;
		case 770: goto st770;
		case 771: goto st771;
		case 772: goto st772;
		case 773: goto st773;
		case 774: goto st774;
		case 775: goto st775;
		case 776: goto st776;
		case 1875: goto st1875;
		case 777: goto st777;
		case 778: goto st778;
		case 779: goto st779;
		case 780: goto st780;
		case 781: goto st781;
		case 782: goto st782;
		case 783: goto st783;
		case 784: goto st784;
		case 1876: goto st1876;
		case 1877: goto st1877;
		case 1878: goto st1878;
		case 1879: goto st1879;
		case 1880: goto st1880;
		case 1881: goto st1881;
		case 1882: goto st1882;
		case 1883: goto st1883;
		case 785: goto st785;
		case 786: goto st786;
		case 1884: goto st1884;
		case 1885: goto st1885;
		case 1886: goto st1886;
		case 1887: goto st1887;
		case 1888: goto st1888;
		case 1889: goto st1889;
		case 787: goto st787;
		case 788: goto st788;
		case 1890: goto st1890;
		case 1891: goto st1891;
		case 1892: goto st1892;
		case 1893: goto st1893;
		case 1894: goto st1894;
		case 1895: goto st1895;
		case 1896: goto st1896;
		case 1897: goto st1897;
		case 1898: goto st1898;
		case 789: goto st789;
		case 790: goto st790;
		case 1899: goto st1899;
		case 1900: goto st1900;
		case 1901: goto st1901;
		case 1902: goto st1902;
		case 1903: goto st1903;
		case 791: goto st791;
		case 792: goto st792;
		case 793: goto st793;
		case 1904: goto st1904;
		case 794: goto st794;
		case 795: goto st795;
		case 796: goto st796;
		case 797: goto st797;
		case 798: goto st798;
		case 799: goto st799;
		case 800: goto st800;
		case 801: goto st801;
		case 802: goto st802;
		case 1905: goto st1905;
		case 803: goto st803;
		case 804: goto st804;
		case 805: goto st805;
		case 806: goto st806;
		case 807: goto st807;
		case 808: goto st808;
		case 1906: goto st1906;
		case 1907: goto st1907;
		case 1908: goto st1908;
		case 1909: goto st1909;
		case 1910: goto st1910;
		case 1911: goto st1911;
		case 1912: goto st1912;
		case 1913: goto st1913;
		case 1914: goto st1914;
		case 1915: goto st1915;
		case 1916: goto st1916;
		case 1917: goto st1917;
		case 1918: goto st1918;
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
		case 1919: goto st1919;
		case 834: goto st834;
		case 835: goto st835;
		case 1920: goto st1920;
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
		case 870: goto st870;
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
		case 1921: goto st1921;
		case 901: goto st901;
		case 1922: goto st1922;
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
		case 1923: goto st1923;
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
		case 1049: goto st1049;
		case 1050: goto st1050;
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
		case 1068: goto st1068;
		case 1069: goto st1069;
		case 1070: goto st1070;
		case 1071: goto st1071;
		case 1072: goto st1072;
		case 1073: goto st1073;
		case 1074: goto st1074;
		case 1924: goto st1924;
		case 1075: goto st1075;
		case 1076: goto st1076;
		case 1925: goto st1925;
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
		case 1926: goto st1926;
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
		case 1927: goto st1927;
		case 1146: goto st1146;
		case 1147: goto st1147;
		case 1148: goto st1148;
		case 1149: goto st1149;
		case 1150: goto st1150;
		case 1151: goto st1151;
		case 1152: goto st1152;
		case 1153: goto st1153;
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
		case 1928: goto st1928;
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
		case 1181: goto st1181;
		case 1182: goto st1182;
		case 1183: goto st1183;
		case 1184: goto st1184;
		case 1185: goto st1185;
		case 1186: goto st1186;
		case 1187: goto st1187;
		case 1188: goto st1188;
		case 1929: goto st1929;
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
		case 1930: goto st1930;
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
		case 1931: goto st1931;
		case 1932: goto st1932;
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
		case 1340: goto st1340;
		case 1341: goto st1341;
		case 1342: goto st1342;
		case 1343: goto st1343;
		case 1344: goto st1344;
		case 1345: goto st1345;
		case 1346: goto st1346;
		case 1347: goto st1347;
		case 1348: goto st1348;
		case 1349: goto st1349;
		case 1350: goto st1350;
		case 1351: goto st1351;
		case 1933: goto st1933;
		case 1352: goto st1352;
		case 1353: goto st1353;
		case 1354: goto st1354;
		case 1355: goto st1355;
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
		case 1388: goto st1388;
		case 1934: goto st1934;
		case 1389: goto st1389;
		case 1390: goto st1390;
		case 1935: goto st1935;
		case 1391: goto st1391;
		case 1392: goto st1392;
		case 1393: goto st1393;
		case 1394: goto st1394;
		case 1936: goto st1936;
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
		case 1937: goto st1937;
		case 1405: goto st1405;
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
		case 1938: goto st1938;
		case 1939: goto st1939;
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
		case 1940: goto st1940;
		case 1941: goto st1941;
		case 1942: goto st1942;
		case 1943: goto st1943;
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
		case 1944: goto st1944;
		case 1945: goto st1945;
		case 1946: goto st1946;
		case 1947: goto st1947;
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
		case 1948: goto st1948;
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
		case 1630: goto st1630;
		case 1631: goto st1631;
		case 1632: goto st1632;
		case 1633: goto st1633;
		case 1634: goto st1634;
		case 1635: goto st1635;
		case 1636: goto st1636;
		case 1637: goto st1637;
		case 1638: goto st1638;
		case 1639: goto st1639;
		case 1640: goto st1640;
		case 1641: goto st1641;
		case 1642: goto st1642;
		case 1643: goto st1643;
		case 1644: goto st1644;
		case 1645: goto st1645;
		case 1646: goto st1646;
		case 1647: goto st1647;
		case 1648: goto st1648;
		case 1649: goto st1649;
		case 1650: goto st1650;
		case 1651: goto st1651;
		case 1652: goto st1652;
		case 1653: goto st1653;
		case 1654: goto st1654;
		case 1655: goto st1655;
		case 1656: goto st1656;
		case 1657: goto st1657;
		case 1658: goto st1658;
		case 1659: goto st1659;
		case 1660: goto st1660;
		case 1661: goto st1661;
		case 1662: goto st1662;
		case 1663: goto st1663;
		case 1664: goto st1664;
		case 1665: goto st1665;
		case 1666: goto st1666;
		case 1667: goto st1667;
		case 1668: goto st1668;
		case 1669: goto st1669;
		case 1670: goto st1670;
		case 1671: goto st1671;
		case 1672: goto st1672;
		case 1673: goto st1673;
		case 1674: goto st1674;
		case 1675: goto st1675;
		case 1676: goto st1676;
		case 1677: goto st1677;
		case 1678: goto st1678;
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
	case 147:
	{{( p) = ((( te)))-1;}
    g_debug("block blank line(s)");
  }
	break;
	case 148:
	{{( p) = ((( te)))-1;}
    g_debug("block char");
    ( p)--;

    if (dstack.empty() || dstack_check(BLOCK_QUOTE) || dstack_check(BLOCK_SPOILER) || dstack_check(BLOCK_SECTION)) {
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
{( (stack.data()))[( top)++] = 1679;goto st1708;}}
  }
	break;
	default:
	{{( p) = ((( te)))-1;}}
	break;
	}
	}
	goto st1679;
tr3:
#line 809 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    g_debug("block char");
    ( p)--;

    if (dstack.empty() || dstack_check(BLOCK_QUOTE) || dstack_check(BLOCK_SPOILER) || dstack_check(BLOCK_SECTION)) {
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
{( (stack.data()))[( top)++] = 1679;goto st1708;}}
  }}
	goto st1679;
tr79:
#line 781 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1946;}}
  }}
	goto st1679;
tr107:
#line 751 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    append_code_fence({ b1, b2 }, { a1, a2 });
  }}
	goto st1679;
tr132:
#line 746 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1938;}}
  }}
	goto st1679;
tr133:
#line 746 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1938;}}
  }}
	goto st1679;
tr135:
#line 741 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1938;}}
  }}
	goto st1679;
tr136:
#line 741 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1938;}}
  }}
	goto st1679;
tr145:
#line 775 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1942;}}
  }}
	goto st1679;
tr146:
#line 775 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1942;}}
  }}
	goto st1679;
tr172:
#line 787 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1708;}}
  }}
	goto st1679;
tr2092:
#line 809 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    g_debug("block char");
    ( p)--;

    if (dstack.empty() || dstack_check(BLOCK_QUOTE) || dstack_check(BLOCK_SPOILER) || dstack_check(BLOCK_SECTION)) {
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
{( (stack.data()))[( top)++] = 1679;goto st1708;}}
  }}
	goto st1679;
tr2103:
#line 809 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("block char");
    ( p)--;

    if (dstack.empty() || dstack_check(BLOCK_QUOTE) || dstack_check(BLOCK_SPOILER) || dstack_check(BLOCK_SECTION)) {
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
{( (stack.data()))[( top)++] = 1679;goto st1708;}}
  }}
	goto st1679;
tr2104:
#line 728 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_QUOTE);
  }}
	goto st1679;
tr2105:
#line 771 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_SECTION);
  }}
	goto st1679;
tr2106:
#line 737 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_SPOILER);
  }}
	goto st1679;
tr2107:
#line 793 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    append_block("<hr>");
  }}
	goto st1679;
tr2108:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 798 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1708;}}
  }}
	goto st1679;
tr2111:
#line 718 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1708;}}
  }}
	goto st1679;
tr2119:
#line 723 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_QUOTE, "<blockquote>");
  }}
	goto st1679;
tr2120:
#line 746 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1938;}}
  }}
	goto st1679;
tr2121:
#line 741 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1938;}}
  }}
	goto st1679;
tr2122:
#line 775 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1679;goto st1942;}}
  }}
	goto st1679;
tr2123:
#line 762 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("block [section=]");
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_SECTION, "<details>");
    append_block("<summary>");
    append_block_html_escaped({ a1, a2 });
    append_block("</summary><div>");
  }}
	goto st1679;
tr2125:
#line 756 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_SECTION, "<details>");
    append_block("<summary>Show</summary><div>");
  }}
	goto st1679;
tr2126:
#line 732 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_SPOILER, "<div class=\"spoiler\">");
  }}
	goto st1679;
st1679:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1679;
case 1679:
#line 1 "NONE"
	{( ts) = ( p);}
#line 3191 "ext/dtext/dtext.cpp"
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
		case 9: goto tr2093;
		case 10: goto tr6;
		case 32: goto tr2093;
		case 42: goto tr2095;
		case 72: goto tr2097;
		case 96: goto tr2098;
		case 104: goto tr2097;
		case 3388: goto tr2099;
		case 3419: goto tr2100;
		case 3644: goto tr2101;
		case 3675: goto tr2102;
	}
	if ( _widec < 14 ) {
		if ( _widec > 8 ) {
			if ( 11 <= _widec && _widec <= 13 )
				goto tr2094;
		} else
			goto tr2092;
	} else if ( _widec > 59 ) {
		if ( _widec > 90 ) {
			if ( 92 <= _widec )
				goto tr2092;
		} else if ( _widec >= 61 )
			goto tr2092;
	} else
		goto tr2092;
	goto st0;
tr1:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 805 "ext/dtext/dtext.cpp.rl"
	{( act) = 147;}
	goto st1680;
tr4:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 804 "ext/dtext/dtext.cpp.rl"
	{( act) = 146;}
	goto st1680;
st1680:
	if ( ++( p) == ( pe) )
		goto _test_eof1680;
case 1680:
#line 3251 "ext/dtext/dtext.cpp"
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
tr2093:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1681;
st1681:
	if ( ++( p) == ( pe) )
		goto _test_eof1681;
case 1681:
#line 3280 "ext/dtext/dtext.cpp"
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
		case 3388: goto st58;
		case 3419: goto st67;
		case 3644: goto st75;
		case 3675: goto st76;
	}
	if ( 11 <= _widec && _widec <= 13 )
		goto st4;
	goto tr2103;
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
		case 3388: goto st58;
		case 3419: goto st67;
		case 3644: goto st75;
		case 3675: goto st76;
	}
	if ( 11 <= _widec && _widec <= 13 )
		goto st4;
	goto tr3;
tr13:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 805 "ext/dtext/dtext.cpp.rl"
	{( act) = 147;}
	goto st1682;
tr6:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 804 "ext/dtext/dtext.cpp.rl"
	{( act) = 146;}
	goto st1682;
st1682:
	if ( ++( p) == ( pe) )
		goto _test_eof1682;
case 1682:
#line 3355 "ext/dtext/dtext.cpp"
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
		case 3419: goto st29;
		case 3644: goto st38;
		case 3675: goto st48;
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
		case 3419: goto st29;
		case 3644: goto st38;
		case 3675: goto st48;
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
		case 3419: goto st29;
		case 3644: goto st38;
		case 3675: goto st48;
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
		case 81: goto st17;
		case 83: goto st22;
		case 98: goto st7;
		case 113: goto st17;
		case 115: goto st22;
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
		goto st1683;
	goto tr0;
st1683:
	if ( ++( p) == ( pe) )
		goto _test_eof1683;
case 1683:
	switch( (*( p)) ) {
		case 9: goto st1683;
		case 32: goto st1683;
	}
	goto tr2104;
st17:
	if ( ++( p) == ( pe) )
		goto _test_eof17;
case 17:
	switch( (*( p)) ) {
		case 85: goto st18;
		case 117: goto st18;
	}
	goto tr0;
st18:
	if ( ++( p) == ( pe) )
		goto _test_eof18;
case 18:
	switch( (*( p)) ) {
		case 79: goto st19;
		case 111: goto st19;
	}
	goto tr0;
st19:
	if ( ++( p) == ( pe) )
		goto _test_eof19;
case 19:
	switch( (*( p)) ) {
		case 84: goto st20;
		case 116: goto st20;
	}
	goto tr0;
st20:
	if ( ++( p) == ( pe) )
		goto _test_eof20;
case 20:
	switch( (*( p)) ) {
		case 69: goto st21;
		case 101: goto st21;
	}
	goto tr0;
st21:
	if ( ++( p) == ( pe) )
		goto _test_eof21;
case 21:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2622 )
		goto st1683;
	goto tr0;
st22:
	if ( ++( p) == ( pe) )
		goto _test_eof22;
case 22:
	switch( (*( p)) ) {
		case 69: goto st23;
		case 101: goto st23;
	}
	goto tr0;
st23:
	if ( ++( p) == ( pe) )
		goto _test_eof23;
case 23:
	switch( (*( p)) ) {
		case 67: goto st24;
		case 99: goto st24;
	}
	goto tr0;
st24:
	if ( ++( p) == ( pe) )
		goto _test_eof24;
case 24:
	switch( (*( p)) ) {
		case 84: goto st25;
		case 116: goto st25;
	}
	goto tr0;
st25:
	if ( ++( p) == ( pe) )
		goto _test_eof25;
case 25:
	switch( (*( p)) ) {
		case 73: goto st26;
		case 105: goto st26;
	}
	goto tr0;
st26:
	if ( ++( p) == ( pe) )
		goto _test_eof26;
case 26:
	switch( (*( p)) ) {
		case 79: goto st27;
		case 111: goto st27;
	}
	goto tr0;
st27:
	if ( ++( p) == ( pe) )
		goto _test_eof27;
case 27:
	switch( (*( p)) ) {
		case 78: goto st28;
		case 110: goto st28;
	}
	goto tr0;
st28:
	if ( ++( p) == ( pe) )
		goto _test_eof28;
case 28:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SECTION)  ) _widec += 256;
	}
	if ( _widec == 3134 )
		goto st1684;
	goto tr0;
st1684:
	if ( ++( p) == ( pe) )
		goto _test_eof1684;
case 1684:
	switch( (*( p)) ) {
		case 9: goto st1684;
		case 32: goto st1684;
	}
	goto tr2105;
st29:
	if ( ++( p) == ( pe) )
		goto _test_eof29;
case 29:
	if ( (*( p)) == 47 )
		goto st30;
	goto tr0;
st30:
	if ( ++( p) == ( pe) )
		goto _test_eof30;
case 30:
	switch( (*( p)) ) {
		case 81: goto st12;
		case 83: goto st31;
		case 113: goto st12;
		case 115: goto st31;
	}
	goto tr0;
st31:
	if ( ++( p) == ( pe) )
		goto _test_eof31;
case 31:
	switch( (*( p)) ) {
		case 69: goto st32;
		case 101: goto st32;
	}
	goto tr0;
st32:
	if ( ++( p) == ( pe) )
		goto _test_eof32;
case 32:
	switch( (*( p)) ) {
		case 67: goto st33;
		case 99: goto st33;
	}
	goto tr0;
st33:
	if ( ++( p) == ( pe) )
		goto _test_eof33;
case 33:
	switch( (*( p)) ) {
		case 84: goto st34;
		case 116: goto st34;
	}
	goto tr0;
st34:
	if ( ++( p) == ( pe) )
		goto _test_eof34;
case 34:
	switch( (*( p)) ) {
		case 73: goto st35;
		case 105: goto st35;
	}
	goto tr0;
st35:
	if ( ++( p) == ( pe) )
		goto _test_eof35;
case 35:
	switch( (*( p)) ) {
		case 79: goto st36;
		case 111: goto st36;
	}
	goto tr0;
st36:
	if ( ++( p) == ( pe) )
		goto _test_eof36;
case 36:
	switch( (*( p)) ) {
		case 78: goto st37;
		case 110: goto st37;
	}
	goto tr0;
st37:
	if ( ++( p) == ( pe) )
		goto _test_eof37;
case 37:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SECTION)  ) _widec += 256;
	}
	if ( _widec == 3165 )
		goto st1684;
	goto tr0;
st38:
	if ( ++( p) == ( pe) )
		goto _test_eof38;
case 38:
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3375: goto st6;
		case 3631: goto st39;
	}
	goto tr0;
st39:
	if ( ++( p) == ( pe) )
		goto _test_eof39;
case 39:
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
		case 81: goto st17;
		case 98: goto st7;
		case 113: goto st17;
		case 3411: goto st22;
		case 3443: goto st22;
		case 3667: goto st40;
		case 3699: goto st40;
	}
	goto tr0;
st40:
	if ( ++( p) == ( pe) )
		goto _test_eof40;
case 40:
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
		case 69: goto st23;
		case 101: goto st23;
		case 3664: goto st41;
		case 3696: goto st41;
	}
	goto tr0;
st41:
	if ( ++( p) == ( pe) )
		goto _test_eof41;
case 41:
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
		case 3663: goto st42;
		case 3695: goto st42;
	}
	goto tr0;
st42:
	if ( ++( p) == ( pe) )
		goto _test_eof42;
case 42:
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
		case 3657: goto st43;
		case 3689: goto st43;
	}
	goto tr0;
st43:
	if ( ++( p) == ( pe) )
		goto _test_eof43;
case 43:
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
		case 3660: goto st44;
		case 3692: goto st44;
	}
	goto tr0;
st44:
	if ( ++( p) == ( pe) )
		goto _test_eof44;
case 44:
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
		case 3653: goto st45;
		case 3685: goto st45;
	}
	goto tr0;
st45:
	if ( ++( p) == ( pe) )
		goto _test_eof45;
case 45:
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
		case 3666: goto st46;
		case 3698: goto st46;
	}
	goto tr0;
st46:
	if ( ++( p) == ( pe) )
		goto _test_eof46;
case 46:
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
		case 3646: goto st1685;
		case 3667: goto st47;
		case 3699: goto st47;
	}
	goto tr0;
st1685:
	if ( ++( p) == ( pe) )
		goto _test_eof1685;
case 1685:
	switch( (*( p)) ) {
		case 9: goto st1685;
		case 32: goto st1685;
	}
	goto tr2106;
st47:
	if ( ++( p) == ( pe) )
		goto _test_eof47;
case 47:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	if ( _widec == 3646 )
		goto st1685;
	goto tr0;
st48:
	if ( ++( p) == ( pe) )
		goto _test_eof48;
case 48:
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 3375: goto st30;
		case 3631: goto st49;
	}
	goto tr0;
st49:
	if ( ++( p) == ( pe) )
		goto _test_eof49;
case 49:
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
		case 81: goto st12;
		case 113: goto st12;
		case 3411: goto st31;
		case 3443: goto st31;
		case 3667: goto st50;
		case 3699: goto st50;
	}
	goto tr0;
st50:
	if ( ++( p) == ( pe) )
		goto _test_eof50;
case 50:
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
		case 69: goto st32;
		case 101: goto st32;
		case 3664: goto st51;
		case 3696: goto st51;
	}
	goto tr0;
st51:
	if ( ++( p) == ( pe) )
		goto _test_eof51;
case 51:
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
		case 3663: goto st52;
		case 3695: goto st52;
	}
	goto tr0;
st52:
	if ( ++( p) == ( pe) )
		goto _test_eof52;
case 52:
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
		case 3657: goto st53;
		case 3689: goto st53;
	}
	goto tr0;
st53:
	if ( ++( p) == ( pe) )
		goto _test_eof53;
case 53:
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
		case 3660: goto st54;
		case 3692: goto st54;
	}
	goto tr0;
st54:
	if ( ++( p) == ( pe) )
		goto _test_eof54;
case 54:
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
		case 3653: goto st55;
		case 3685: goto st55;
	}
	goto tr0;
st55:
	if ( ++( p) == ( pe) )
		goto _test_eof55;
case 55:
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
		case 3666: goto st56;
		case 3698: goto st56;
	}
	goto tr0;
st56:
	if ( ++( p) == ( pe) )
		goto _test_eof56;
case 56:
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
		case 3667: goto st57;
		case 3677: goto st1685;
		case 3699: goto st57;
	}
	goto tr0;
st57:
	if ( ++( p) == ( pe) )
		goto _test_eof57;
case 57:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	if ( _widec == 3677 )
		goto st1685;
	goto tr0;
st58:
	if ( ++( p) == ( pe) )
		goto _test_eof58;
case 58:
	switch( (*( p)) ) {
		case 47: goto st6;
		case 72: goto st59;
		case 84: goto st62;
		case 104: goto st59;
		case 116: goto st62;
	}
	goto tr3;
st59:
	if ( ++( p) == ( pe) )
		goto _test_eof59;
case 59:
	switch( (*( p)) ) {
		case 82: goto st60;
		case 114: goto st60;
	}
	goto tr3;
st60:
	if ( ++( p) == ( pe) )
		goto _test_eof60;
case 60:
	if ( (*( p)) == 62 )
		goto st61;
	goto tr3;
st61:
	if ( ++( p) == ( pe) )
		goto _test_eof61;
case 61:
	switch( (*( p)) ) {
		case 0: goto st1686;
		case 9: goto st61;
		case 10: goto st1686;
		case 32: goto st61;
	}
	goto tr3;
st1686:
	if ( ++( p) == ( pe) )
		goto _test_eof1686;
case 1686:
	switch( (*( p)) ) {
		case 0: goto st1686;
		case 10: goto st1686;
	}
	goto tr2107;
st62:
	if ( ++( p) == ( pe) )
		goto _test_eof62;
case 62:
	switch( (*( p)) ) {
		case 65: goto st63;
		case 97: goto st63;
	}
	goto tr3;
st63:
	if ( ++( p) == ( pe) )
		goto _test_eof63;
case 63:
	switch( (*( p)) ) {
		case 66: goto st64;
		case 98: goto st64;
	}
	goto tr3;
st64:
	if ( ++( p) == ( pe) )
		goto _test_eof64;
case 64:
	switch( (*( p)) ) {
		case 76: goto st65;
		case 108: goto st65;
	}
	goto tr3;
st65:
	if ( ++( p) == ( pe) )
		goto _test_eof65;
case 65:
	switch( (*( p)) ) {
		case 69: goto st66;
		case 101: goto st66;
	}
	goto tr3;
st66:
	if ( ++( p) == ( pe) )
		goto _test_eof66;
case 66:
	if ( (*( p)) == 62 )
		goto tr79;
	goto tr3;
st67:
	if ( ++( p) == ( pe) )
		goto _test_eof67;
case 67:
	switch( (*( p)) ) {
		case 47: goto st30;
		case 72: goto st68;
		case 84: goto st70;
		case 104: goto st68;
		case 116: goto st70;
	}
	goto tr3;
st68:
	if ( ++( p) == ( pe) )
		goto _test_eof68;
case 68:
	switch( (*( p)) ) {
		case 82: goto st69;
		case 114: goto st69;
	}
	goto tr3;
st69:
	if ( ++( p) == ( pe) )
		goto _test_eof69;
case 69:
	if ( (*( p)) == 93 )
		goto st61;
	goto tr3;
st70:
	if ( ++( p) == ( pe) )
		goto _test_eof70;
case 70:
	switch( (*( p)) ) {
		case 65: goto st71;
		case 97: goto st71;
	}
	goto tr3;
st71:
	if ( ++( p) == ( pe) )
		goto _test_eof71;
case 71:
	switch( (*( p)) ) {
		case 66: goto st72;
		case 98: goto st72;
	}
	goto tr3;
st72:
	if ( ++( p) == ( pe) )
		goto _test_eof72;
case 72:
	switch( (*( p)) ) {
		case 76: goto st73;
		case 108: goto st73;
	}
	goto tr3;
st73:
	if ( ++( p) == ( pe) )
		goto _test_eof73;
case 73:
	switch( (*( p)) ) {
		case 69: goto st74;
		case 101: goto st74;
	}
	goto tr3;
st74:
	if ( ++( p) == ( pe) )
		goto _test_eof74;
case 74:
	if ( (*( p)) == 93 )
		goto tr79;
	goto tr3;
st75:
	if ( ++( p) == ( pe) )
		goto _test_eof75;
case 75:
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 72: goto st59;
		case 84: goto st62;
		case 104: goto st59;
		case 116: goto st62;
		case 3375: goto st6;
		case 3631: goto st39;
	}
	goto tr3;
st76:
	if ( ++( p) == ( pe) )
		goto _test_eof76;
case 76:
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 72: goto st68;
		case 84: goto st70;
		case 104: goto st68;
		case 116: goto st70;
		case 3375: goto st30;
		case 3631: goto st49;
	}
	goto tr3;
tr2094:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1687;
st1687:
	if ( ++( p) == ( pe) )
		goto _test_eof1687;
case 1687:
#line 4458 "ext/dtext/dtext.cpp"
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
		case 3419: goto st29;
		case 3644: goto st38;
		case 3675: goto st48;
	}
	if ( 9 <= _widec && _widec <= 13 )
		goto st4;
	goto tr2103;
tr2095:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1688;
st1688:
	if ( ++( p) == ( pe) )
		goto _test_eof1688;
case 1688:
#line 4493 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr89;
		case 32: goto tr89;
		case 42: goto st78;
	}
	goto tr2103;
tr89:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
	goto st77;
st77:
	if ( ++( p) == ( pe) )
		goto _test_eof77;
case 77:
#line 4508 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr88;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr88;
	}
	goto tr87;
tr87:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1689;
st1689:
	if ( ++( p) == ( pe) )
		goto _test_eof1689;
case 1689:
#line 4525 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2108;
		case 10: goto tr2108;
		case 13: goto tr2108;
	}
	goto st1689;
tr88:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1690;
st1690:
	if ( ++( p) == ( pe) )
		goto _test_eof1690;
case 1690:
#line 4540 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2108;
		case 9: goto tr88;
		case 10: goto tr2108;
		case 13: goto tr2108;
		case 32: goto tr88;
	}
	goto tr87;
st78:
	if ( ++( p) == ( pe) )
		goto _test_eof78;
case 78:
	switch( (*( p)) ) {
		case 9: goto tr89;
		case 32: goto tr89;
		case 42: goto st78;
	}
	goto tr3;
st0:
( cs) = 0;
	goto _out;
tr2097:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1691;
st1691:
	if ( ++( p) == ( pe) )
		goto _test_eof1691;
case 1691:
#line 4570 "ext/dtext/dtext.cpp"
	if ( 49 <= (*( p)) && (*( p)) <= 54 )
		goto tr2110;
	goto tr2103;
tr2110:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st79;
st79:
	if ( ++( p) == ( pe) )
		goto _test_eof79;
case 79:
#line 4582 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr91;
		case 46: goto tr92;
	}
	goto tr3;
tr91:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st80;
st80:
	if ( ++( p) == ( pe) )
		goto _test_eof80;
case 80:
#line 4596 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto tr93;
		case 35: goto tr93;
		case 38: goto tr93;
		case 45: goto tr93;
		case 95: goto tr93;
	}
	if ( (*( p)) < 65 ) {
		if ( 47 <= (*( p)) && (*( p)) <= 58 )
			goto tr93;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr93;
	} else
		goto tr93;
	goto tr3;
tr93:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st81;
st81:
	if ( ++( p) == ( pe) )
		goto _test_eof81;
case 81:
#line 4621 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto st81;
		case 35: goto st81;
		case 38: goto st81;
		case 46: goto tr95;
		case 95: goto st81;
	}
	if ( (*( p)) < 65 ) {
		if ( 45 <= (*( p)) && (*( p)) <= 58 )
			goto st81;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st81;
	} else
		goto st81;
	goto tr3;
tr92:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1692;
tr95:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1692;
st1692:
	if ( ++( p) == ( pe) )
		goto _test_eof1692;
case 1692:
#line 4654 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1692;
		case 32: goto st1692;
	}
	goto tr2111;
tr2098:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1693;
st1693:
	if ( ++( p) == ( pe) )
		goto _test_eof1693;
case 1693:
#line 4668 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 96 )
		goto st82;
	goto tr2103;
st82:
	if ( ++( p) == ( pe) )
		goto _test_eof82;
case 82:
	if ( (*( p)) == 96 )
		goto st83;
	goto tr3;
tr98:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st83;
st83:
	if ( ++( p) == ( pe) )
		goto _test_eof83;
case 83:
#line 4689 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr97;
		case 9: goto tr98;
		case 10: goto tr97;
		case 32: goto tr98;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr99;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr99;
	} else
		goto tr99;
	goto tr3;
tr108:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st84;
tr97:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st84;
st84:
	if ( ++( p) == ( pe) )
		goto _test_eof84;
case 84:
#line 4719 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr101;
		case 10: goto tr101;
	}
	goto tr100;
tr100:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st85;
st85:
	if ( ++( p) == ( pe) )
		goto _test_eof85;
case 85:
#line 4733 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr103;
		case 10: goto tr103;
	}
	goto st85;
tr103:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st86;
tr101:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st86;
st86:
	if ( ++( p) == ( pe) )
		goto _test_eof86;
case 86:
#line 4753 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr103;
		case 10: goto tr103;
		case 96: goto st87;
	}
	goto st85;
st87:
	if ( ++( p) == ( pe) )
		goto _test_eof87;
case 87:
	switch( (*( p)) ) {
		case 0: goto tr103;
		case 10: goto tr103;
		case 96: goto st88;
	}
	goto st85;
st88:
	if ( ++( p) == ( pe) )
		goto _test_eof88;
case 88:
	switch( (*( p)) ) {
		case 0: goto tr103;
		case 10: goto tr103;
		case 96: goto st89;
	}
	goto st85;
st89:
	if ( ++( p) == ( pe) )
		goto _test_eof89;
case 89:
	switch( (*( p)) ) {
		case 0: goto tr107;
		case 9: goto st89;
		case 10: goto tr107;
		case 32: goto st89;
	}
	goto st85;
tr99:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st90;
st90:
	if ( ++( p) == ( pe) )
		goto _test_eof90;
case 90:
#line 4799 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr108;
		case 9: goto tr109;
		case 10: goto tr108;
		case 32: goto tr109;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st90;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st90;
	} else
		goto st90;
	goto tr3;
tr109:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st91;
st91:
	if ( ++( p) == ( pe) )
		goto _test_eof91;
case 91:
#line 4823 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st84;
		case 9: goto st91;
		case 10: goto st84;
		case 32: goto st91;
	}
	goto tr3;
tr2099:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1694;
st1694:
	if ( ++( p) == ( pe) )
		goto _test_eof1694;
case 1694:
#line 4841 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st6;
		case 66: goto st92;
		case 67: goto st102;
		case 72: goto st59;
		case 78: goto st111;
		case 81: goto st97;
		case 83: goto st119;
		case 84: goto st136;
		case 98: goto st92;
		case 99: goto st102;
		case 104: goto st59;
		case 110: goto st111;
		case 113: goto st97;
		case 115: goto st119;
		case 116: goto st136;
	}
	goto tr2103;
st92:
	if ( ++( p) == ( pe) )
		goto _test_eof92;
case 92:
	switch( (*( p)) ) {
		case 76: goto st93;
		case 108: goto st93;
	}
	goto tr3;
st93:
	if ( ++( p) == ( pe) )
		goto _test_eof93;
case 93:
	switch( (*( p)) ) {
		case 79: goto st94;
		case 111: goto st94;
	}
	goto tr3;
st94:
	if ( ++( p) == ( pe) )
		goto _test_eof94;
case 94:
	switch( (*( p)) ) {
		case 67: goto st95;
		case 99: goto st95;
	}
	goto tr3;
st95:
	if ( ++( p) == ( pe) )
		goto _test_eof95;
case 95:
	switch( (*( p)) ) {
		case 75: goto st96;
		case 107: goto st96;
	}
	goto tr3;
st96:
	if ( ++( p) == ( pe) )
		goto _test_eof96;
case 96:
	switch( (*( p)) ) {
		case 81: goto st97;
		case 113: goto st97;
	}
	goto tr3;
st97:
	if ( ++( p) == ( pe) )
		goto _test_eof97;
case 97:
	switch( (*( p)) ) {
		case 85: goto st98;
		case 117: goto st98;
	}
	goto tr3;
st98:
	if ( ++( p) == ( pe) )
		goto _test_eof98;
case 98:
	switch( (*( p)) ) {
		case 79: goto st99;
		case 111: goto st99;
	}
	goto tr3;
st99:
	if ( ++( p) == ( pe) )
		goto _test_eof99;
case 99:
	switch( (*( p)) ) {
		case 84: goto st100;
		case 116: goto st100;
	}
	goto tr3;
st100:
	if ( ++( p) == ( pe) )
		goto _test_eof100;
case 100:
	switch( (*( p)) ) {
		case 69: goto st101;
		case 101: goto st101;
	}
	goto tr3;
st101:
	if ( ++( p) == ( pe) )
		goto _test_eof101;
case 101:
	if ( (*( p)) == 62 )
		goto st1695;
	goto tr3;
st1695:
	if ( ++( p) == ( pe) )
		goto _test_eof1695;
case 1695:
	if ( (*( p)) == 32 )
		goto st1695;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1695;
	goto tr2119;
st102:
	if ( ++( p) == ( pe) )
		goto _test_eof102;
case 102:
	switch( (*( p)) ) {
		case 79: goto st103;
		case 111: goto st103;
	}
	goto tr3;
st103:
	if ( ++( p) == ( pe) )
		goto _test_eof103;
case 103:
	switch( (*( p)) ) {
		case 68: goto st104;
		case 100: goto st104;
	}
	goto tr3;
st104:
	if ( ++( p) == ( pe) )
		goto _test_eof104;
case 104:
	switch( (*( p)) ) {
		case 69: goto st105;
		case 101: goto st105;
	}
	goto tr3;
st105:
	if ( ++( p) == ( pe) )
		goto _test_eof105;
case 105:
	switch( (*( p)) ) {
		case 9: goto st106;
		case 32: goto st106;
		case 61: goto st107;
		case 62: goto tr128;
	}
	goto tr3;
st106:
	if ( ++( p) == ( pe) )
		goto _test_eof106;
case 106:
	switch( (*( p)) ) {
		case 9: goto st106;
		case 32: goto st106;
		case 61: goto st107;
	}
	goto tr3;
st107:
	if ( ++( p) == ( pe) )
		goto _test_eof107;
case 107:
	switch( (*( p)) ) {
		case 9: goto st107;
		case 32: goto st107;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr129;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr129;
	} else
		goto tr129;
	goto tr3;
tr129:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st108;
st108:
	if ( ++( p) == ( pe) )
		goto _test_eof108;
case 108:
#line 5030 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 62 )
		goto tr131;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st108;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st108;
	} else
		goto st108;
	goto tr3;
tr131:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1696;
st1696:
	if ( ++( p) == ( pe) )
		goto _test_eof1696;
case 1696:
#line 5052 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr133;
		case 9: goto st109;
		case 10: goto tr133;
		case 32: goto st109;
	}
	goto tr2120;
st109:
	if ( ++( p) == ( pe) )
		goto _test_eof109;
case 109:
	switch( (*( p)) ) {
		case 0: goto tr133;
		case 9: goto st109;
		case 10: goto tr133;
		case 32: goto st109;
	}
	goto tr132;
tr128:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1697;
st1697:
	if ( ++( p) == ( pe) )
		goto _test_eof1697;
case 1697:
#line 5079 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr136;
		case 9: goto st110;
		case 10: goto tr136;
		case 32: goto st110;
	}
	goto tr2121;
st110:
	if ( ++( p) == ( pe) )
		goto _test_eof110;
case 110:
	switch( (*( p)) ) {
		case 0: goto tr136;
		case 9: goto st110;
		case 10: goto tr136;
		case 32: goto st110;
	}
	goto tr135;
st111:
	if ( ++( p) == ( pe) )
		goto _test_eof111;
case 111:
	switch( (*( p)) ) {
		case 79: goto st112;
		case 111: goto st112;
	}
	goto tr3;
st112:
	if ( ++( p) == ( pe) )
		goto _test_eof112;
case 112:
	switch( (*( p)) ) {
		case 68: goto st113;
		case 100: goto st113;
	}
	goto tr3;
st113:
	if ( ++( p) == ( pe) )
		goto _test_eof113;
case 113:
	switch( (*( p)) ) {
		case 84: goto st114;
		case 116: goto st114;
	}
	goto tr3;
st114:
	if ( ++( p) == ( pe) )
		goto _test_eof114;
case 114:
	switch( (*( p)) ) {
		case 69: goto st115;
		case 101: goto st115;
	}
	goto tr3;
st115:
	if ( ++( p) == ( pe) )
		goto _test_eof115;
case 115:
	switch( (*( p)) ) {
		case 88: goto st116;
		case 120: goto st116;
	}
	goto tr3;
st116:
	if ( ++( p) == ( pe) )
		goto _test_eof116;
case 116:
	switch( (*( p)) ) {
		case 84: goto st117;
		case 116: goto st117;
	}
	goto tr3;
st117:
	if ( ++( p) == ( pe) )
		goto _test_eof117;
case 117:
	if ( (*( p)) == 62 )
		goto tr144;
	goto tr3;
tr144:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1698;
st1698:
	if ( ++( p) == ( pe) )
		goto _test_eof1698;
case 1698:
#line 5167 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr146;
		case 9: goto st118;
		case 10: goto tr146;
		case 32: goto st118;
	}
	goto tr2122;
st118:
	if ( ++( p) == ( pe) )
		goto _test_eof118;
case 118:
	switch( (*( p)) ) {
		case 0: goto tr146;
		case 9: goto st118;
		case 10: goto tr146;
		case 32: goto st118;
	}
	goto tr145;
st119:
	if ( ++( p) == ( pe) )
		goto _test_eof119;
case 119:
	switch( (*( p)) ) {
		case 69: goto st120;
		case 80: goto st129;
		case 101: goto st120;
		case 112: goto st129;
	}
	goto tr3;
st120:
	if ( ++( p) == ( pe) )
		goto _test_eof120;
case 120:
	switch( (*( p)) ) {
		case 67: goto st121;
		case 99: goto st121;
	}
	goto tr3;
st121:
	if ( ++( p) == ( pe) )
		goto _test_eof121;
case 121:
	switch( (*( p)) ) {
		case 84: goto st122;
		case 116: goto st122;
	}
	goto tr3;
st122:
	if ( ++( p) == ( pe) )
		goto _test_eof122;
case 122:
	switch( (*( p)) ) {
		case 73: goto st123;
		case 105: goto st123;
	}
	goto tr3;
st123:
	if ( ++( p) == ( pe) )
		goto _test_eof123;
case 123:
	switch( (*( p)) ) {
		case 79: goto st124;
		case 111: goto st124;
	}
	goto tr3;
st124:
	if ( ++( p) == ( pe) )
		goto _test_eof124;
case 124:
	switch( (*( p)) ) {
		case 78: goto st125;
		case 110: goto st125;
	}
	goto tr3;
st125:
	if ( ++( p) == ( pe) )
		goto _test_eof125;
case 125:
	switch( (*( p)) ) {
		case 9: goto st126;
		case 32: goto st126;
		case 61: goto st128;
		case 62: goto st1700;
	}
	goto tr3;
tr159:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st126;
st126:
	if ( ++( p) == ( pe) )
		goto _test_eof126;
case 126:
#line 5261 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr159;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr159;
		case 61: goto tr160;
		case 62: goto tr161;
	}
	goto tr158;
tr158:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st127;
st127:
	if ( ++( p) == ( pe) )
		goto _test_eof127;
case 127:
#line 5280 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 10: goto tr3;
		case 13: goto tr3;
		case 62: goto tr163;
	}
	goto st127;
tr163:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1699;
tr161:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1699;
st1699:
	if ( ++( p) == ( pe) )
		goto _test_eof1699;
case 1699:
#line 5302 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 32 )
		goto st1699;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1699;
	goto tr2123;
tr160:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st128;
st128:
	if ( ++( p) == ( pe) )
		goto _test_eof128;
case 128:
#line 5316 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr160;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr160;
		case 62: goto tr161;
	}
	goto tr158;
st1700:
	if ( ++( p) == ( pe) )
		goto _test_eof1700;
case 1700:
	if ( (*( p)) == 32 )
		goto st1700;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1700;
	goto tr2125;
st129:
	if ( ++( p) == ( pe) )
		goto _test_eof129;
case 129:
	switch( (*( p)) ) {
		case 79: goto st130;
		case 111: goto st130;
	}
	goto tr3;
st130:
	if ( ++( p) == ( pe) )
		goto _test_eof130;
case 130:
	switch( (*( p)) ) {
		case 73: goto st131;
		case 105: goto st131;
	}
	goto tr3;
st131:
	if ( ++( p) == ( pe) )
		goto _test_eof131;
case 131:
	switch( (*( p)) ) {
		case 76: goto st132;
		case 108: goto st132;
	}
	goto tr3;
st132:
	if ( ++( p) == ( pe) )
		goto _test_eof132;
case 132:
	switch( (*( p)) ) {
		case 69: goto st133;
		case 101: goto st133;
	}
	goto tr3;
st133:
	if ( ++( p) == ( pe) )
		goto _test_eof133;
case 133:
	switch( (*( p)) ) {
		case 82: goto st134;
		case 114: goto st134;
	}
	goto tr3;
st134:
	if ( ++( p) == ( pe) )
		goto _test_eof134;
case 134:
	switch( (*( p)) ) {
		case 62: goto st1701;
		case 83: goto st135;
		case 115: goto st135;
	}
	goto tr3;
st1701:
	if ( ++( p) == ( pe) )
		goto _test_eof1701;
case 1701:
	if ( (*( p)) == 32 )
		goto st1701;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1701;
	goto tr2126;
st135:
	if ( ++( p) == ( pe) )
		goto _test_eof135;
case 135:
	if ( (*( p)) == 62 )
		goto st1701;
	goto tr3;
st136:
	if ( ++( p) == ( pe) )
		goto _test_eof136;
case 136:
	switch( (*( p)) ) {
		case 65: goto st63;
		case 78: goto st137;
		case 97: goto st63;
		case 110: goto st137;
	}
	goto tr3;
st137:
	if ( ++( p) == ( pe) )
		goto _test_eof137;
case 137:
	if ( (*( p)) == 62 )
		goto tr172;
	goto tr3;
tr2100:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1702;
st1702:
	if ( ++( p) == ( pe) )
		goto _test_eof1702;
case 1702:
#line 5434 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st30;
		case 67: goto st138;
		case 72: goto st68;
		case 78: goto st145;
		case 81: goto st152;
		case 83: goto st157;
		case 84: goto st174;
		case 99: goto st138;
		case 104: goto st68;
		case 110: goto st145;
		case 113: goto st152;
		case 115: goto st157;
		case 116: goto st174;
	}
	goto tr2103;
st138:
	if ( ++( p) == ( pe) )
		goto _test_eof138;
case 138:
	switch( (*( p)) ) {
		case 79: goto st139;
		case 111: goto st139;
	}
	goto tr3;
st139:
	if ( ++( p) == ( pe) )
		goto _test_eof139;
case 139:
	switch( (*( p)) ) {
		case 68: goto st140;
		case 100: goto st140;
	}
	goto tr3;
st140:
	if ( ++( p) == ( pe) )
		goto _test_eof140;
case 140:
	switch( (*( p)) ) {
		case 69: goto st141;
		case 101: goto st141;
	}
	goto tr3;
st141:
	if ( ++( p) == ( pe) )
		goto _test_eof141;
case 141:
	switch( (*( p)) ) {
		case 9: goto st142;
		case 32: goto st142;
		case 61: goto st143;
		case 93: goto tr128;
	}
	goto tr3;
st142:
	if ( ++( p) == ( pe) )
		goto _test_eof142;
case 142:
	switch( (*( p)) ) {
		case 9: goto st142;
		case 32: goto st142;
		case 61: goto st143;
	}
	goto tr3;
st143:
	if ( ++( p) == ( pe) )
		goto _test_eof143;
case 143:
	switch( (*( p)) ) {
		case 9: goto st143;
		case 32: goto st143;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr178;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr178;
	} else
		goto tr178;
	goto tr3;
tr178:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st144;
st144:
	if ( ++( p) == ( pe) )
		goto _test_eof144;
case 144:
#line 5524 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto tr131;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st144;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st144;
	} else
		goto st144;
	goto tr3;
st145:
	if ( ++( p) == ( pe) )
		goto _test_eof145;
case 145:
	switch( (*( p)) ) {
		case 79: goto st146;
		case 111: goto st146;
	}
	goto tr3;
st146:
	if ( ++( p) == ( pe) )
		goto _test_eof146;
case 146:
	switch( (*( p)) ) {
		case 68: goto st147;
		case 100: goto st147;
	}
	goto tr3;
st147:
	if ( ++( p) == ( pe) )
		goto _test_eof147;
case 147:
	switch( (*( p)) ) {
		case 84: goto st148;
		case 116: goto st148;
	}
	goto tr3;
st148:
	if ( ++( p) == ( pe) )
		goto _test_eof148;
case 148:
	switch( (*( p)) ) {
		case 69: goto st149;
		case 101: goto st149;
	}
	goto tr3;
st149:
	if ( ++( p) == ( pe) )
		goto _test_eof149;
case 149:
	switch( (*( p)) ) {
		case 88: goto st150;
		case 120: goto st150;
	}
	goto tr3;
st150:
	if ( ++( p) == ( pe) )
		goto _test_eof150;
case 150:
	switch( (*( p)) ) {
		case 84: goto st151;
		case 116: goto st151;
	}
	goto tr3;
st151:
	if ( ++( p) == ( pe) )
		goto _test_eof151;
case 151:
	if ( (*( p)) == 93 )
		goto tr144;
	goto tr3;
st152:
	if ( ++( p) == ( pe) )
		goto _test_eof152;
case 152:
	switch( (*( p)) ) {
		case 85: goto st153;
		case 117: goto st153;
	}
	goto tr3;
st153:
	if ( ++( p) == ( pe) )
		goto _test_eof153;
case 153:
	switch( (*( p)) ) {
		case 79: goto st154;
		case 111: goto st154;
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
	if ( (*( p)) == 93 )
		goto st1695;
	goto tr3;
st157:
	if ( ++( p) == ( pe) )
		goto _test_eof157;
case 157:
	switch( (*( p)) ) {
		case 69: goto st158;
		case 80: goto st167;
		case 101: goto st158;
		case 112: goto st167;
	}
	goto tr3;
st158:
	if ( ++( p) == ( pe) )
		goto _test_eof158;
case 158:
	switch( (*( p)) ) {
		case 67: goto st159;
		case 99: goto st159;
	}
	goto tr3;
st159:
	if ( ++( p) == ( pe) )
		goto _test_eof159;
case 159:
	switch( (*( p)) ) {
		case 84: goto st160;
		case 116: goto st160;
	}
	goto tr3;
st160:
	if ( ++( p) == ( pe) )
		goto _test_eof160;
case 160:
	switch( (*( p)) ) {
		case 73: goto st161;
		case 105: goto st161;
	}
	goto tr3;
st161:
	if ( ++( p) == ( pe) )
		goto _test_eof161;
case 161:
	switch( (*( p)) ) {
		case 79: goto st162;
		case 111: goto st162;
	}
	goto tr3;
st162:
	if ( ++( p) == ( pe) )
		goto _test_eof162;
case 162:
	switch( (*( p)) ) {
		case 78: goto st163;
		case 110: goto st163;
	}
	goto tr3;
st163:
	if ( ++( p) == ( pe) )
		goto _test_eof163;
case 163:
	switch( (*( p)) ) {
		case 9: goto st164;
		case 32: goto st164;
		case 61: goto st166;
		case 93: goto st1700;
	}
	goto tr3;
tr200:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st164;
st164:
	if ( ++( p) == ( pe) )
		goto _test_eof164;
case 164:
#line 5715 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr200;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr200;
		case 61: goto tr201;
		case 93: goto tr161;
	}
	goto tr199;
tr199:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st165;
st165:
	if ( ++( p) == ( pe) )
		goto _test_eof165;
case 165:
#line 5734 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 10: goto tr3;
		case 13: goto tr3;
		case 93: goto tr163;
	}
	goto st165;
tr201:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st166;
st166:
	if ( ++( p) == ( pe) )
		goto _test_eof166;
case 166:
#line 5750 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr201;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr201;
		case 93: goto tr161;
	}
	goto tr199;
st167:
	if ( ++( p) == ( pe) )
		goto _test_eof167;
case 167:
	switch( (*( p)) ) {
		case 79: goto st168;
		case 111: goto st168;
	}
	goto tr3;
st168:
	if ( ++( p) == ( pe) )
		goto _test_eof168;
case 168:
	switch( (*( p)) ) {
		case 73: goto st169;
		case 105: goto st169;
	}
	goto tr3;
st169:
	if ( ++( p) == ( pe) )
		goto _test_eof169;
case 169:
	switch( (*( p)) ) {
		case 76: goto st170;
		case 108: goto st170;
	}
	goto tr3;
st170:
	if ( ++( p) == ( pe) )
		goto _test_eof170;
case 170:
	switch( (*( p)) ) {
		case 69: goto st171;
		case 101: goto st171;
	}
	goto tr3;
st171:
	if ( ++( p) == ( pe) )
		goto _test_eof171;
case 171:
	switch( (*( p)) ) {
		case 82: goto st172;
		case 114: goto st172;
	}
	goto tr3;
st172:
	if ( ++( p) == ( pe) )
		goto _test_eof172;
case 172:
	switch( (*( p)) ) {
		case 83: goto st173;
		case 93: goto st1701;
		case 115: goto st173;
	}
	goto tr3;
st173:
	if ( ++( p) == ( pe) )
		goto _test_eof173;
case 173:
	if ( (*( p)) == 93 )
		goto st1701;
	goto tr3;
st174:
	if ( ++( p) == ( pe) )
		goto _test_eof174;
case 174:
	switch( (*( p)) ) {
		case 65: goto st71;
		case 78: goto st175;
		case 97: goto st71;
		case 110: goto st175;
	}
	goto tr3;
st175:
	if ( ++( p) == ( pe) )
		goto _test_eof175;
case 175:
	if ( (*( p)) == 93 )
		goto tr172;
	goto tr3;
tr2101:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1703;
st1703:
	if ( ++( p) == ( pe) )
		goto _test_eof1703;
case 1703:
#line 5850 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 66: goto st92;
		case 67: goto st102;
		case 72: goto st59;
		case 78: goto st111;
		case 81: goto st97;
		case 83: goto st119;
		case 84: goto st136;
		case 98: goto st92;
		case 99: goto st102;
		case 104: goto st59;
		case 110: goto st111;
		case 113: goto st97;
		case 115: goto st119;
		case 116: goto st136;
		case 3375: goto st6;
		case 3631: goto st39;
	}
	goto tr2103;
tr2102:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1704;
st1704:
	if ( ++( p) == ( pe) )
		goto _test_eof1704;
case 1704:
#line 5887 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( 47 <= (*( p)) && (*( p)) <= 47 ) {
		_widec = (short)(3200 + ((*( p)) - -128));
		if ( 
#line 92 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SPOILER)  ) _widec += 256;
	}
	switch( _widec ) {
		case 67: goto st138;
		case 72: goto st68;
		case 78: goto st145;
		case 81: goto st152;
		case 83: goto st157;
		case 84: goto st174;
		case 99: goto st138;
		case 104: goto st68;
		case 110: goto st145;
		case 113: goto st152;
		case 115: goto st157;
		case 116: goto st174;
		case 3375: goto st30;
		case 3631: goto st49;
	}
	goto tr2103;
tr210:
#line 315 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_html_escaped((*( p))); }}
	goto st1705;
tr216:
#line 303 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_B, { ts, te }); }}
	goto st1705;
tr217:
#line 305 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_I, { ts, te }); }}
	goto st1705;
tr218:
#line 307 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_S, { ts, te }); }}
	goto st1705;
tr226:
#line 313 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_SUB, { ts, te }); }}
	goto st1705;
tr227:
#line 311 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_SUP, { ts, te }); }}
	goto st1705;
tr228:
#line 309 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_U, { ts, te }); }}
	goto st1705;
tr229:
#line 302 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_B, "<strong>"); }}
	goto st1705;
tr231:
#line 304 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_I, "<em>"); }}
	goto st1705;
tr232:
#line 306 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_S, "<s>"); }}
	goto st1705;
tr241:
#line 312 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_SUB, "<sub>"); }}
	goto st1705;
tr242:
#line 310 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_SUP, "<sup>"); }}
	goto st1705;
tr243:
#line 308 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_U, "<u>"); }}
	goto st1705;
tr2132:
#line 315 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append_html_escaped((*( p))); }}
	goto st1705;
tr2133:
#line 314 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1705;
tr2136:
#line 315 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_html_escaped((*( p))); }}
	goto st1705;
st1705:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1705;
case 1705:
#line 1 "NONE"
	{( ts) = ( p);}
#line 5984 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2133;
		case 60: goto tr2134;
		case 91: goto tr2135;
	}
	goto tr2132;
tr2134:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1706;
st1706:
	if ( ++( p) == ( pe) )
		goto _test_eof1706;
case 1706:
#line 5999 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st176;
		case 66: goto st189;
		case 69: goto st190;
		case 73: goto st191;
		case 83: goto st192;
		case 85: goto st200;
		case 98: goto st189;
		case 101: goto st190;
		case 105: goto st191;
		case 115: goto st192;
		case 117: goto st200;
	}
	goto tr2136;
st176:
	if ( ++( p) == ( pe) )
		goto _test_eof176;
case 176:
	switch( (*( p)) ) {
		case 66: goto st177;
		case 69: goto st178;
		case 73: goto st179;
		case 83: goto st180;
		case 85: goto st188;
		case 98: goto st177;
		case 101: goto st178;
		case 105: goto st179;
		case 115: goto st180;
		case 117: goto st188;
	}
	goto tr210;
st177:
	if ( ++( p) == ( pe) )
		goto _test_eof177;
case 177:
	if ( (*( p)) == 62 )
		goto tr216;
	goto tr210;
st178:
	if ( ++( p) == ( pe) )
		goto _test_eof178;
case 178:
	switch( (*( p)) ) {
		case 77: goto st179;
		case 109: goto st179;
	}
	goto tr210;
st179:
	if ( ++( p) == ( pe) )
		goto _test_eof179;
case 179:
	if ( (*( p)) == 62 )
		goto tr217;
	goto tr210;
st180:
	if ( ++( p) == ( pe) )
		goto _test_eof180;
case 180:
	switch( (*( p)) ) {
		case 62: goto tr218;
		case 84: goto st181;
		case 85: goto st185;
		case 116: goto st181;
		case 117: goto st185;
	}
	goto tr210;
st181:
	if ( ++( p) == ( pe) )
		goto _test_eof181;
case 181:
	switch( (*( p)) ) {
		case 82: goto st182;
		case 114: goto st182;
	}
	goto tr210;
st182:
	if ( ++( p) == ( pe) )
		goto _test_eof182;
case 182:
	switch( (*( p)) ) {
		case 79: goto st183;
		case 111: goto st183;
	}
	goto tr210;
st183:
	if ( ++( p) == ( pe) )
		goto _test_eof183;
case 183:
	switch( (*( p)) ) {
		case 78: goto st184;
		case 110: goto st184;
	}
	goto tr210;
st184:
	if ( ++( p) == ( pe) )
		goto _test_eof184;
case 184:
	switch( (*( p)) ) {
		case 71: goto st177;
		case 103: goto st177;
	}
	goto tr210;
st185:
	if ( ++( p) == ( pe) )
		goto _test_eof185;
case 185:
	switch( (*( p)) ) {
		case 66: goto st186;
		case 80: goto st187;
		case 98: goto st186;
		case 112: goto st187;
	}
	goto tr210;
st186:
	if ( ++( p) == ( pe) )
		goto _test_eof186;
case 186:
	if ( (*( p)) == 62 )
		goto tr226;
	goto tr210;
st187:
	if ( ++( p) == ( pe) )
		goto _test_eof187;
case 187:
	if ( (*( p)) == 62 )
		goto tr227;
	goto tr210;
st188:
	if ( ++( p) == ( pe) )
		goto _test_eof188;
case 188:
	if ( (*( p)) == 62 )
		goto tr228;
	goto tr210;
st189:
	if ( ++( p) == ( pe) )
		goto _test_eof189;
case 189:
	if ( (*( p)) == 62 )
		goto tr229;
	goto tr210;
st190:
	if ( ++( p) == ( pe) )
		goto _test_eof190;
case 190:
	switch( (*( p)) ) {
		case 77: goto st191;
		case 109: goto st191;
	}
	goto tr210;
st191:
	if ( ++( p) == ( pe) )
		goto _test_eof191;
case 191:
	if ( (*( p)) == 62 )
		goto tr231;
	goto tr210;
st192:
	if ( ++( p) == ( pe) )
		goto _test_eof192;
case 192:
	switch( (*( p)) ) {
		case 62: goto tr232;
		case 84: goto st193;
		case 85: goto st197;
		case 116: goto st193;
		case 117: goto st197;
	}
	goto tr210;
st193:
	if ( ++( p) == ( pe) )
		goto _test_eof193;
case 193:
	switch( (*( p)) ) {
		case 82: goto st194;
		case 114: goto st194;
	}
	goto tr210;
st194:
	if ( ++( p) == ( pe) )
		goto _test_eof194;
case 194:
	switch( (*( p)) ) {
		case 79: goto st195;
		case 111: goto st195;
	}
	goto tr210;
st195:
	if ( ++( p) == ( pe) )
		goto _test_eof195;
case 195:
	switch( (*( p)) ) {
		case 78: goto st196;
		case 110: goto st196;
	}
	goto tr210;
st196:
	if ( ++( p) == ( pe) )
		goto _test_eof196;
case 196:
	switch( (*( p)) ) {
		case 71: goto st189;
		case 103: goto st189;
	}
	goto tr210;
st197:
	if ( ++( p) == ( pe) )
		goto _test_eof197;
case 197:
	switch( (*( p)) ) {
		case 66: goto st198;
		case 80: goto st199;
		case 98: goto st198;
		case 112: goto st199;
	}
	goto tr210;
st198:
	if ( ++( p) == ( pe) )
		goto _test_eof198;
case 198:
	if ( (*( p)) == 62 )
		goto tr241;
	goto tr210;
st199:
	if ( ++( p) == ( pe) )
		goto _test_eof199;
case 199:
	if ( (*( p)) == 62 )
		goto tr242;
	goto tr210;
st200:
	if ( ++( p) == ( pe) )
		goto _test_eof200;
case 200:
	if ( (*( p)) == 62 )
		goto tr243;
	goto tr210;
tr2135:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1707;
st1707:
	if ( ++( p) == ( pe) )
		goto _test_eof1707;
case 1707:
#line 6245 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st201;
		case 66: goto st209;
		case 73: goto st210;
		case 83: goto st211;
		case 85: goto st215;
		case 98: goto st209;
		case 105: goto st210;
		case 115: goto st211;
		case 117: goto st215;
	}
	goto tr2136;
st201:
	if ( ++( p) == ( pe) )
		goto _test_eof201;
case 201:
	switch( (*( p)) ) {
		case 66: goto st202;
		case 73: goto st203;
		case 83: goto st204;
		case 85: goto st208;
		case 98: goto st202;
		case 105: goto st203;
		case 115: goto st204;
		case 117: goto st208;
	}
	goto tr210;
st202:
	if ( ++( p) == ( pe) )
		goto _test_eof202;
case 202:
	if ( (*( p)) == 93 )
		goto tr216;
	goto tr210;
st203:
	if ( ++( p) == ( pe) )
		goto _test_eof203;
case 203:
	if ( (*( p)) == 93 )
		goto tr217;
	goto tr210;
st204:
	if ( ++( p) == ( pe) )
		goto _test_eof204;
case 204:
	switch( (*( p)) ) {
		case 85: goto st205;
		case 93: goto tr218;
		case 117: goto st205;
	}
	goto tr210;
st205:
	if ( ++( p) == ( pe) )
		goto _test_eof205;
case 205:
	switch( (*( p)) ) {
		case 66: goto st206;
		case 80: goto st207;
		case 98: goto st206;
		case 112: goto st207;
	}
	goto tr210;
st206:
	if ( ++( p) == ( pe) )
		goto _test_eof206;
case 206:
	if ( (*( p)) == 93 )
		goto tr226;
	goto tr210;
st207:
	if ( ++( p) == ( pe) )
		goto _test_eof207;
case 207:
	if ( (*( p)) == 93 )
		goto tr227;
	goto tr210;
st208:
	if ( ++( p) == ( pe) )
		goto _test_eof208;
case 208:
	if ( (*( p)) == 93 )
		goto tr228;
	goto tr210;
st209:
	if ( ++( p) == ( pe) )
		goto _test_eof209;
case 209:
	if ( (*( p)) == 93 )
		goto tr229;
	goto tr210;
st210:
	if ( ++( p) == ( pe) )
		goto _test_eof210;
case 210:
	if ( (*( p)) == 93 )
		goto tr231;
	goto tr210;
st211:
	if ( ++( p) == ( pe) )
		goto _test_eof211;
case 211:
	switch( (*( p)) ) {
		case 85: goto st212;
		case 93: goto tr232;
		case 117: goto st212;
	}
	goto tr210;
st212:
	if ( ++( p) == ( pe) )
		goto _test_eof212;
case 212:
	switch( (*( p)) ) {
		case 66: goto st213;
		case 80: goto st214;
		case 98: goto st213;
		case 112: goto st214;
	}
	goto tr210;
st213:
	if ( ++( p) == ( pe) )
		goto _test_eof213;
case 213:
	if ( (*( p)) == 93 )
		goto tr241;
	goto tr210;
st214:
	if ( ++( p) == ( pe) )
		goto _test_eof214;
case 214:
	if ( (*( p)) == 93 )
		goto tr242;
	goto tr210;
st215:
	if ( ++( p) == ( pe) )
		goto _test_eof215;
case 215:
	if ( (*( p)) == 93 )
		goto tr243;
	goto tr210;
tr254:
#line 1 "NONE"
	{	switch( ( act) ) {
	case 52:
	{{( p) = ((( te)))-1;}
    append_bare_named_url({ b1, b2 + 1 }, { a1, a2 });
  }
	break;
	case 53:
	{{( p) = ((( te)))-1;}
    append_named_url({ b1, b2 }, { a1, a2 });
  }
	break;
	case 54:
	{{( p) = ((( te)))-1;}
    append_named_url({ a1, a2 }, { b1, b2 });
  }
	break;
	case 55:
	{{( p) = ((( te)))-1;}
    append_named_url({ g1, g2 }, { f1, f2 });
  }
	break;
	case 56:
	{{( p) = ((( te)))-1;}
    append_bare_unnamed_url({ ts, te });
  }
	break;
	case 58:
	{{( p) = ((( te)))-1;}
    append_mention({ a1, a2 + 1 });
  }
	break;
	case 68:
	{{( p) = ((( te)))-1;} dstack_open_element(INLINE_SUP, "<sup>"); }
	break;
	case 69:
	{{( p) = ((( te)))-1;} dstack_close_element(INLINE_SUP, { ts, te }); }
	break;
	case 70:
	{{( p) = ((( te)))-1;} dstack_open_element(INLINE_SUB, "<sub>"); }
	break;
	case 71:
	{{( p) = ((( te)))-1;} dstack_close_element(INLINE_SUB, { ts, te }); }
	break;
	case 75:
	{{( p) = ((( te)))-1;}
    if(options.f_allow_color) {
      dstack_open_element(INLINE_COLOR, "<span class=\"dtext-color-");
      append_uri_escaped({ a1, a2 });
      append("\">");
    }
  }
	break;
	case 76:
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
	case 77:
	{{( p) = ((( te)))-1;}
    if(options.f_allow_color) {
      dstack_close_element(INLINE_COLOR, { ts, te });
    }
  }
	break;
	case 78:
	{{( p) = ((( te)))-1;}
    append("<a id=\"");
    std::string lowercased_tag = std::string(b1, b2 - b1);
    std::transform(lowercased_tag.begin(), lowercased_tag.end(), lowercased_tag.begin(), [](unsigned char c) { return std::tolower(c); });
    append_uri_escaped(lowercased_tag);
    append("\"></a>");
  }
	break;
	case 80:
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
{( (stack.data()))[( top)++] = 1708;goto st1938;}}
  }
	break;
	case 91:
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
	case 92:
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
	case 109:
	{{( p) = ((( te)))-1;}
    append({ ts, te });
  }
	break;
	case 110:
	{{( p) = ((( te)))-1;}
    append_html_escaped((*( p)));
  }
	break;
	default:
	{{( p) = ((( te)))-1;}}
	break;
	}
	}
	goto st1708;
tr257:
#line 625 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append({ ts, te });
  }}
	goto st1708;
tr261:
#line 629 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1708;
tr263:
#line 589 "ext/dtext/dtext.cpp.rl"
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
	goto st1708;
tr299:
#line 565 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TD, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1708;
tr300:
#line 559 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TH, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1708;
tr309:
#line 548 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( a1))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr321:
#line 508 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr348:
#line 571 "ext/dtext/dtext.cpp.rl"
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
	goto st1708;
tr352:
#line 445 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    g_debug("inline [/tn]");

    if (dstack_check(INLINE_TN)) {
      dstack_close_element(INLINE_TN, { ts, te });
    } else if (dstack_close_element(BLOCK_TN, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1708;
tr373:
#line 518 "ext/dtext/dtext.cpp.rl"
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
	goto st1708;
tr380:
#line 537 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr383:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 537 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr401:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 537 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr478:
#line 394 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_bare_named_url({ b1, b2 + 1 }, { a1, a2 });
  }}
	goto st1708;
tr544:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 398 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ b1, b2 }, { a1, a2 });
  }}
	goto st1708;
tr554:
#line 608 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("'"); }}
	goto st1708;
tr559:
#line 604 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&amp;"); }}
	goto st1708;
tr562:
#line 609 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("'"); }}
	goto st1708;
tr564:
#line 612 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('*'); }}
	goto st1708;
tr570:
#line 613 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append(':'); }}
	goto st1708;
tr574:
#line 614 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('@'); }}
	goto st1708;
tr580:
#line 615 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('`'); }}
	goto st1708;
tr581:
#line 606 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&gt;"); }}
	goto st1708;
tr589:
#line 610 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('{'); }}
	goto st1708;
tr590:
#line 611 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('['); }}
	goto st1708;
tr591:
#line 605 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&lt;"); }}
	goto st1708;
tr594:
#line 616 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('#'); }}
	goto st1708;
tr600:
#line 617 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('.'); }}
	goto st1708;
tr604:
#line 607 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&quot;"); }}
	goto st1708;
tr830:
#line 345 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_id_link("dmail", "dmail", "/dmails/", { a1, a2 }); }}
	goto st1708;
tr851:
#line 342 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_id_link("topic", "forum-topic", "/forum_topics/", { a1, a2 }); }}
	goto st1708;
tr877:
#line 410 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_bare_unnamed_url({ ts, te });
  }}
	goto st1708;
tr994:
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ g1, g2 }, { f1, f2 });
  }}
	goto st1708;
tr1010:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 402 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
	goto st1708;
tr1012:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 402 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
	goto st1708;
tr1033:
#line 429 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_B, { ts, te }); }}
	goto st1708;
tr1044:
#line 431 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_I, { ts, te }); }}
	goto st1708;
tr1057:
#line 433 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_S, { ts, te }); }}
	goto st1708;
tr1084:
#line 435 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_U, { ts, te }); }}
	goto st1708;
tr1086:
#line 428 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_B, "<strong>"); }}
	goto st1708;
tr1087:
#line 455 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (header_mode) {
      append_html_escaped("<br>");
    } else {
      append("<br>");
    };
  }}
	goto st1708;
tr1098:
#line 503 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1938;}}
  }}
	goto st1708;
tr1099:
#line 503 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1938;}}
  }}
	goto st1708;
tr1101:
#line 498 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1938;}}
  }}
	goto st1708;
tr1102:
#line 498 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1938;}}
  }}
	goto st1708;
tr1292:
#line 430 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_I, "<em>"); }}
	goto st1708;
tr1300:
#line 529 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1942;}}
  }}
	goto st1708;
tr1301:
#line 529 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1942;}}
  }}
	goto st1708;
tr1310:
#line 432 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_S, "<s>"); }}
	goto st1708;
tr1321:
#line 514 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element(INLINE_SPOILER, "<span class=\"spoiler\">");
  }}
	goto st1708;
tr1336:
#line 441 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element(INLINE_TN, "<span class=\"tn\">");
  }}
	goto st1708;
tr1338:
#line 434 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_U, "<u>"); }}
	goto st1708;
tr1366:
#line 398 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ b1, b2 }, { a1, a2 });
  }}
	goto st1708;
tr1470:
#line 414 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_unnamed_url({ a1, a2 });
  }}
	goto st1708;
tr1597:
#line 439 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_SUB, { ts, te }); }}
	goto st1708;
tr1598:
#line 437 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_SUP, { ts, te }); }}
	goto st1708;
tr1616:
#line 402 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
	goto st1708;
tr1642:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 414 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_unnamed_url({ a1, a2 });
  }}
	goto st1708;
tr1664:
#line 438 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_SUB, "<sub>"); }}
	goto st1708;
tr1665:
#line 436 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_SUP, "<sup>"); }}
	goto st1708;
tr1669:
#line 418 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_mention({ a1, a2 + 1 });
  }}
	goto st1708;
tr2146:
#line 629 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1708;
tr2153:
#line 619 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append(' ');
  }}
	goto st1708;
tr2177:
#line 629 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1708;
tr2178:
#line 625 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append({ ts, te });
  }}
	goto st1708;
tr2180:
#line 589 "ext/dtext/dtext.cpp.rl"
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
	goto st1708;
tr2187:
#line 543 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_QUOTE);
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr2188:
#line 554 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_SECTION);
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr2189:
#line 537 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr2190:
#line 571 "ext/dtext/dtext.cpp.rl"
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
	goto st1708;
tr2193:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 422 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("inline list");
    {( p) = (( ts + 1))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1708;
tr2197:
#line 394 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_bare_named_url({ b1, b2 + 1 }, { a1, a2 });
  }}
	goto st1708;
tr2209:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 386 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1708;
tr2211:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 386 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1708;
tr2213:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 390 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1708;
tr2215:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 390 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1708;
tr2219:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 378 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_internal_anchor_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1708;
tr2221:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 378 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_internal_anchor_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1708;
tr2223:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 382 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_internal_anchor_link({ a1, a2 }, { b1, b2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1708;
tr2225:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 382 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_internal_anchor_link({ a1, a2 }, { b1, b2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1708;
tr2227:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 374 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 });
  }}
	goto st1708;
tr2229:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 374 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 });
  }}
	goto st1708;
tr2231:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 370 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { d1, d2 });
  }}
	goto st1708;
tr2233:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 370 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { d1, d2 });
  }}
	goto st1708;
tr2245:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 353 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("alias", "tag-alias", "/tags/aliases/", { a1, a2 }); }}
	goto st1708;
tr2252:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 349 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("artist", "artist", "/artists/", { a1, a2 }); }}
	goto st1708;
tr2254:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 350 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("artist changes", "artist-changes-for", "/artists/versions?search[artist_id]=", { a1, a2 }); }}
	goto st1708;
tr2260:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 363 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("avoid posting", "avoid-posting", "/avoid_postings/", { a1, a2 }); }}
	goto st1708;
tr2266:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 351 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("ban", "ban", "/bans/", { a1, a2 }); }}
	goto st1708;
tr2270:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 352 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("BUR", "bulk-update-request", "/bulk_update_requests/", { a1, a2 }); }}
	goto st1708;
tr2280:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 344 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("comment", "comment", "/comments/", { a1, a2 }); }}
	goto st1708;
tr2284:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 367 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("commit", "github-commit", "https://github.com/PawsMovin/PawsMovin/commit/", { a1, a2 }); }}
	goto st1708;
tr2292:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 345 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("dmail", "dmail", "/dmails/", { a1, a2 }); }}
	goto st1708;
tr2295:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 346 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_dmail_key_link({ a1, a2 }, { b1, b2 }); }}
	goto st1708;
tr2299:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 362 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("avoid posting", "avoid-posting", "/avoid_postings/", { a1, a2 }); }}
	goto st1708;
tr2306:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 339 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("flag", "post-flag", "/posts/flags/", { a1, a2 }); }}
	goto st1708;
tr2313:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 341 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("forum", "forum-post", "/forum_posts/", { a1, a2 }); }}
	goto st1708;
tr2315:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 342 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("topic", "forum-topic", "/forum_topics/", { a1, a2 }); }}
	goto st1708;
tr2318:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 343 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_paged_link("topic #", { a1, a2 }, "<a class=\"dtext-link dtext-id-link dtext-forum-topic-id-link\" href=\"", "/forum_topics/", "?page=", { b1, b2 }); }}
	goto st1708;
tr2328:
#line 410 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_bare_unnamed_url({ ts, te });
  }}
	goto st1708;
tr2341:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 354 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("implication", "tag-implication", "/tags/implications/", { a1, a2 }); }}
	goto st1708;
tr2347:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 365 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("issue", "github", "https://github.com/PawsMovin/PawsMovin/issues/", { a1, a2 }); }}
	goto st1708;
tr2352:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 355 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("mod action", "mod-action", "/mod_actions/", { a1, a2 }); }}
	goto st1708;
tr2358:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 340 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("note", "note", "/notes/", { a1, a2 }); }}
	goto st1708;
tr2366:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 347 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("pool", "pool", "/pools/", { a1, a2 }); }}
	goto st1708;
tr2370:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 337 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("post", "post", "/posts/", { a1, a2 }); }}
	goto st1708;
tr2372:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 338 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("post changes", "post-changes-for", "/posts/versions?search[post_id]=", { a1, a2 }); }}
	goto st1708;
tr2377:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 366 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("pull", "github-pull", "https://github.com/PawsMovin/PawsMovin/pull/", { a1, a2 }); }}
	goto st1708;
tr2385:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 356 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("record", "user-feedback", "/users/feedbacks/", { a1, a2 }); }}
	goto st1708;
tr2390:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 359 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("set", "set", "/post_sets/", { a1, a2 }); }}
	goto st1708;
tr2400:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 361 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("takedown", "takedown", "/takedowns/", { a1, a2 }); }}
	goto st1708;
tr2409:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 319 "ext/dtext/dtext.cpp.rl"
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
	goto st1708;
tr2416:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 360 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("ticket", "ticket", "/tickets/", { a1, a2 }); }}
	goto st1708;
tr2425:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 348 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("user", "user", "/users/", { a1, a2 }); }}
	goto st1708;
tr2433:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 357 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("wiki", "wiki-page", "/wiki_pages/", { a1, a2 }); }}
	goto st1708;
tr2435:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 358 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("wiki changes", "wiki-page-changes-for", "/wiki_pages/versions?search[wiki_page_id]=", { a1, a2 }); }}
	goto st1708;
tr2460:
#line 503 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1938;}}
  }}
	goto st1708;
tr2461:
#line 498 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1938;}}
  }}
	goto st1708;
tr2462:
#line 406 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_named_url({ g1, g2 }, { f1, f2 });
  }}
	goto st1708;
tr2463:
#line 529 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1708;goto st1942;}}
  }}
	goto st1708;
tr2483:
#line 418 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_mention({ a1, a2 + 1 });
  }}
	goto st1708;
st1708:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1708;
case 1708:
#line 1 "NONE"
	{( ts) = ( p);}
#line 7489 "ext/dtext/dtext.cpp"
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
		case 0: goto tr2150;
		case 9: goto tr2151;
		case 10: goto tr2152;
		case 13: goto tr2153;
		case 32: goto tr2151;
		case 34: goto tr2154;
		case 38: goto tr2155;
		case 65: goto tr2157;
		case 66: goto tr2158;
		case 67: goto tr2159;
		case 68: goto tr2160;
		case 70: goto tr2161;
		case 72: goto tr2162;
		case 73: goto tr2163;
		case 77: goto tr2164;
		case 78: goto tr2165;
		case 80: goto tr2166;
		case 82: goto tr2167;
		case 83: goto tr2168;
		case 84: goto tr2169;
		case 85: goto tr2170;
		case 87: goto tr2171;
		case 91: goto tr2172;
		case 97: goto tr2157;
		case 98: goto tr2158;
		case 99: goto tr2159;
		case 100: goto tr2160;
		case 102: goto tr2161;
		case 104: goto tr2162;
		case 105: goto tr2163;
		case 109: goto tr2164;
		case 110: goto tr2165;
		case 112: goto tr2166;
		case 114: goto tr2167;
		case 115: goto tr2168;
		case 116: goto tr2169;
		case 117: goto tr2170;
		case 119: goto tr2171;
		case 123: goto tr2173;
		case 828: goto tr2174;
		case 1084: goto tr2175;
		case 1344: goto tr2146;
		case 1600: goto tr2146;
		case 1856: goto tr2146;
		case 2112: goto tr2176;
	}
	if ( _widec < 48 ) {
		if ( _widec < -32 ) {
			if ( _widec > -63 ) {
				if ( -62 <= _widec && _widec <= -33 )
					goto st1709;
			} else
				goto tr2146;
		} else if ( _widec > -17 ) {
			if ( _widec > -12 ) {
				if ( -11 <= _widec && _widec <= 47 )
					goto tr2146;
			} else if ( _widec >= -16 )
				goto tr2149;
		} else
			goto tr2148;
	} else if ( _widec > 57 ) {
		if ( _widec < 69 ) {
			if ( _widec > 59 ) {
				if ( 61 <= _widec && _widec <= 63 )
					goto tr2146;
			} else if ( _widec >= 58 )
				goto tr2146;
		} else if ( _widec > 90 ) {
			if ( _widec < 101 ) {
				if ( 92 <= _widec && _widec <= 96 )
					goto tr2146;
			} else if ( _widec > 122 ) {
				if ( 124 <= _widec )
					goto tr2146;
			} else
				goto tr2156;
		} else
			goto tr2156;
	} else
		goto tr2156;
	goto st0;
st1709:
	if ( ++( p) == ( pe) )
		goto _test_eof1709;
case 1709:
	if ( (*( p)) <= -65 )
		goto tr255;
	goto tr2177;
tr255:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1710;
st1710:
	if ( ++( p) == ( pe) )
		goto _test_eof1710;
case 1710:
#line 7607 "ext/dtext/dtext.cpp"
	if ( (*( p)) < -32 ) {
		if ( -62 <= (*( p)) && (*( p)) <= -33 )
			goto st216;
	} else if ( (*( p)) > -17 ) {
		if ( -16 <= (*( p)) && (*( p)) <= -12 )
			goto st218;
	} else
		goto st217;
	goto tr2178;
st216:
	if ( ++( p) == ( pe) )
		goto _test_eof216;
case 216:
	if ( (*( p)) <= -65 )
		goto tr255;
	goto tr254;
st217:
	if ( ++( p) == ( pe) )
		goto _test_eof217;
case 217:
	if ( (*( p)) <= -65 )
		goto st216;
	goto tr254;
st218:
	if ( ++( p) == ( pe) )
		goto _test_eof218;
case 218:
	if ( (*( p)) <= -65 )
		goto st217;
	goto tr257;
tr2148:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1711;
st1711:
	if ( ++( p) == ( pe) )
		goto _test_eof1711;
case 1711:
#line 7648 "ext/dtext/dtext.cpp"
	if ( (*( p)) <= -65 )
		goto st216;
	goto tr2177;
tr2149:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1712;
st1712:
	if ( ++( p) == ( pe) )
		goto _test_eof1712;
case 1712:
#line 7662 "ext/dtext/dtext.cpp"
	if ( (*( p)) <= -65 )
		goto st217;
	goto tr2177;
tr259:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 571 "ext/dtext/dtext.cpp.rl"
	{( act) = 91;}
	goto st1713;
tr2150:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 623 "ext/dtext/dtext.cpp.rl"
	{( act) = 108;}
	goto st1713;
st1713:
	if ( ++( p) == ( pe) )
		goto _test_eof1713;
case 1713:
#line 7682 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto st219;
		case 10: goto tr259;
		case 32: goto st219;
	}
	goto tr254;
st219:
	if ( ++( p) == ( pe) )
		goto _test_eof219;
case 219:
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto st219;
		case 10: goto tr259;
		case 32: goto st219;
	}
	goto tr254;
tr2151:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1714;
st1714:
	if ( ++( p) == ( pe) )
		goto _test_eof1714;
case 1714:
#line 7711 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st219;
		case 9: goto st220;
		case 10: goto st219;
		case 32: goto st220;
	}
	goto tr2177;
st220:
	if ( ++( p) == ( pe) )
		goto _test_eof220;
case 220:
	switch( (*( p)) ) {
		case 0: goto st219;
		case 9: goto st220;
		case 10: goto st219;
		case 32: goto st220;
	}
	goto tr261;
tr2152:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 589 "ext/dtext/dtext.cpp.rl"
	{( act) = 92;}
	goto st1715;
st1715:
	if ( ++( p) == ( pe) )
		goto _test_eof1715;
case 1715:
#line 7740 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto st221;
		case 10: goto tr2181;
		case 32: goto st221;
		case 42: goto tr2182;
		case 60: goto tr2183;
		case 72: goto st347;
		case 91: goto tr2185;
		case 96: goto st390;
		case 104: goto st347;
	}
	goto tr2180;
st221:
	if ( ++( p) == ( pe) )
		goto _test_eof221;
case 221:
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto st221;
		case 10: goto tr259;
		case 32: goto st221;
		case 60: goto tr265;
		case 91: goto tr266;
	}
	goto tr263;
tr265:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st222;
st222:
	if ( ++( p) == ( pe) )
		goto _test_eof222;
case 222:
#line 7775 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st223;
		case 66: goto st249;
		case 72: goto st259;
		case 81: goto st254;
		case 83: goto st262;
		case 98: goto st249;
		case 104: goto st259;
		case 113: goto st254;
		case 115: goto st262;
	}
	goto tr263;
st223:
	if ( ++( p) == ( pe) )
		goto _test_eof223;
case 223:
	switch( (*( p)) ) {
		case 66: goto st224;
		case 81: goto st234;
		case 83: goto st239;
		case 84: goto st246;
		case 98: goto st224;
		case 113: goto st234;
		case 115: goto st239;
		case 116: goto st246;
	}
	goto tr263;
st224:
	if ( ++( p) == ( pe) )
		goto _test_eof224;
case 224:
	switch( (*( p)) ) {
		case 76: goto st225;
		case 108: goto st225;
	}
	goto tr263;
st225:
	if ( ++( p) == ( pe) )
		goto _test_eof225;
case 225:
	switch( (*( p)) ) {
		case 79: goto st226;
		case 111: goto st226;
	}
	goto tr254;
st226:
	if ( ++( p) == ( pe) )
		goto _test_eof226;
case 226:
	switch( (*( p)) ) {
		case 67: goto st227;
		case 99: goto st227;
	}
	goto tr254;
st227:
	if ( ++( p) == ( pe) )
		goto _test_eof227;
case 227:
	switch( (*( p)) ) {
		case 75: goto st228;
		case 107: goto st228;
	}
	goto tr254;
st228:
	if ( ++( p) == ( pe) )
		goto _test_eof228;
case 228:
	switch( (*( p)) ) {
		case 81: goto st229;
		case 113: goto st229;
	}
	goto tr254;
st229:
	if ( ++( p) == ( pe) )
		goto _test_eof229;
case 229:
	switch( (*( p)) ) {
		case 85: goto st230;
		case 117: goto st230;
	}
	goto tr254;
st230:
	if ( ++( p) == ( pe) )
		goto _test_eof230;
case 230:
	switch( (*( p)) ) {
		case 79: goto st231;
		case 111: goto st231;
	}
	goto tr254;
st231:
	if ( ++( p) == ( pe) )
		goto _test_eof231;
case 231:
	switch( (*( p)) ) {
		case 84: goto st232;
		case 116: goto st232;
	}
	goto tr254;
st232:
	if ( ++( p) == ( pe) )
		goto _test_eof232;
case 232:
	switch( (*( p)) ) {
		case 69: goto st233;
		case 101: goto st233;
	}
	goto tr254;
st233:
	if ( ++( p) == ( pe) )
		goto _test_eof233;
case 233:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2653 )
		goto st1716;
	goto tr254;
st1716:
	if ( ++( p) == ( pe) )
		goto _test_eof1716;
case 1716:
	switch( (*( p)) ) {
		case 9: goto st1716;
		case 32: goto st1716;
	}
	goto tr2187;
st234:
	if ( ++( p) == ( pe) )
		goto _test_eof234;
case 234:
	switch( (*( p)) ) {
		case 85: goto st235;
		case 117: goto st235;
	}
	goto tr254;
st235:
	if ( ++( p) == ( pe) )
		goto _test_eof235;
case 235:
	switch( (*( p)) ) {
		case 79: goto st236;
		case 111: goto st236;
	}
	goto tr254;
st236:
	if ( ++( p) == ( pe) )
		goto _test_eof236;
case 236:
	switch( (*( p)) ) {
		case 84: goto st237;
		case 116: goto st237;
	}
	goto tr254;
st237:
	if ( ++( p) == ( pe) )
		goto _test_eof237;
case 237:
	switch( (*( p)) ) {
		case 69: goto st238;
		case 101: goto st238;
	}
	goto tr254;
st238:
	if ( ++( p) == ( pe) )
		goto _test_eof238;
case 238:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2622 )
		goto st1716;
	goto tr254;
st239:
	if ( ++( p) == ( pe) )
		goto _test_eof239;
case 239:
	switch( (*( p)) ) {
		case 69: goto st240;
		case 101: goto st240;
	}
	goto tr263;
st240:
	if ( ++( p) == ( pe) )
		goto _test_eof240;
case 240:
	switch( (*( p)) ) {
		case 67: goto st241;
		case 99: goto st241;
	}
	goto tr254;
st241:
	if ( ++( p) == ( pe) )
		goto _test_eof241;
case 241:
	switch( (*( p)) ) {
		case 84: goto st242;
		case 116: goto st242;
	}
	goto tr254;
st242:
	if ( ++( p) == ( pe) )
		goto _test_eof242;
case 242:
	switch( (*( p)) ) {
		case 73: goto st243;
		case 105: goto st243;
	}
	goto tr254;
st243:
	if ( ++( p) == ( pe) )
		goto _test_eof243;
case 243:
	switch( (*( p)) ) {
		case 79: goto st244;
		case 111: goto st244;
	}
	goto tr254;
st244:
	if ( ++( p) == ( pe) )
		goto _test_eof244;
case 244:
	switch( (*( p)) ) {
		case 78: goto st245;
		case 110: goto st245;
	}
	goto tr254;
st245:
	if ( ++( p) == ( pe) )
		goto _test_eof245;
case 245:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SECTION)  ) _widec += 256;
	}
	if ( _widec == 3134 )
		goto st1717;
	goto tr254;
st1717:
	if ( ++( p) == ( pe) )
		goto _test_eof1717;
case 1717:
	switch( (*( p)) ) {
		case 9: goto st1717;
		case 32: goto st1717;
	}
	goto tr2188;
st246:
	if ( ++( p) == ( pe) )
		goto _test_eof246;
case 246:
	switch( (*( p)) ) {
		case 68: goto st247;
		case 72: goto st248;
		case 100: goto st247;
		case 104: goto st248;
	}
	goto tr263;
st247:
	if ( ++( p) == ( pe) )
		goto _test_eof247;
case 247:
	if ( (*( p)) == 62 )
		goto tr299;
	goto tr254;
st248:
	if ( ++( p) == ( pe) )
		goto _test_eof248;
case 248:
	if ( (*( p)) == 62 )
		goto tr300;
	goto tr254;
st249:
	if ( ++( p) == ( pe) )
		goto _test_eof249;
case 249:
	switch( (*( p)) ) {
		case 76: goto st250;
		case 108: goto st250;
	}
	goto tr263;
st250:
	if ( ++( p) == ( pe) )
		goto _test_eof250;
case 250:
	switch( (*( p)) ) {
		case 79: goto st251;
		case 111: goto st251;
	}
	goto tr254;
st251:
	if ( ++( p) == ( pe) )
		goto _test_eof251;
case 251:
	switch( (*( p)) ) {
		case 67: goto st252;
		case 99: goto st252;
	}
	goto tr254;
st252:
	if ( ++( p) == ( pe) )
		goto _test_eof252;
case 252:
	switch( (*( p)) ) {
		case 75: goto st253;
		case 107: goto st253;
	}
	goto tr254;
st253:
	if ( ++( p) == ( pe) )
		goto _test_eof253;
case 253:
	switch( (*( p)) ) {
		case 81: goto st254;
		case 113: goto st254;
	}
	goto tr254;
st254:
	if ( ++( p) == ( pe) )
		goto _test_eof254;
case 254:
	switch( (*( p)) ) {
		case 85: goto st255;
		case 117: goto st255;
	}
	goto tr254;
st255:
	if ( ++( p) == ( pe) )
		goto _test_eof255;
case 255:
	switch( (*( p)) ) {
		case 79: goto st256;
		case 111: goto st256;
	}
	goto tr254;
st256:
	if ( ++( p) == ( pe) )
		goto _test_eof256;
case 256:
	switch( (*( p)) ) {
		case 84: goto st257;
		case 116: goto st257;
	}
	goto tr254;
st257:
	if ( ++( p) == ( pe) )
		goto _test_eof257;
case 257:
	switch( (*( p)) ) {
		case 69: goto st258;
		case 101: goto st258;
	}
	goto tr254;
st258:
	if ( ++( p) == ( pe) )
		goto _test_eof258;
case 258:
	if ( (*( p)) == 62 )
		goto tr309;
	goto tr254;
st259:
	if ( ++( p) == ( pe) )
		goto _test_eof259;
case 259:
	switch( (*( p)) ) {
		case 82: goto st260;
		case 114: goto st260;
	}
	goto tr263;
st260:
	if ( ++( p) == ( pe) )
		goto _test_eof260;
case 260:
	if ( (*( p)) == 62 )
		goto st261;
	goto tr263;
st261:
	if ( ++( p) == ( pe) )
		goto _test_eof261;
case 261:
	switch( (*( p)) ) {
		case 0: goto st1718;
		case 9: goto st261;
		case 10: goto st1718;
		case 32: goto st261;
	}
	goto tr263;
st1718:
	if ( ++( p) == ( pe) )
		goto _test_eof1718;
case 1718:
	switch( (*( p)) ) {
		case 0: goto st1718;
		case 10: goto st1718;
	}
	goto tr2189;
st262:
	if ( ++( p) == ( pe) )
		goto _test_eof262;
case 262:
	switch( (*( p)) ) {
		case 80: goto st263;
		case 112: goto st263;
	}
	goto tr263;
st263:
	if ( ++( p) == ( pe) )
		goto _test_eof263;
case 263:
	switch( (*( p)) ) {
		case 79: goto st264;
		case 111: goto st264;
	}
	goto tr263;
st264:
	if ( ++( p) == ( pe) )
		goto _test_eof264;
case 264:
	switch( (*( p)) ) {
		case 73: goto st265;
		case 105: goto st265;
	}
	goto tr263;
st265:
	if ( ++( p) == ( pe) )
		goto _test_eof265;
case 265:
	switch( (*( p)) ) {
		case 76: goto st266;
		case 108: goto st266;
	}
	goto tr263;
st266:
	if ( ++( p) == ( pe) )
		goto _test_eof266;
case 266:
	switch( (*( p)) ) {
		case 69: goto st267;
		case 101: goto st267;
	}
	goto tr263;
st267:
	if ( ++( p) == ( pe) )
		goto _test_eof267;
case 267:
	switch( (*( p)) ) {
		case 82: goto st268;
		case 114: goto st268;
	}
	goto tr263;
st268:
	if ( ++( p) == ( pe) )
		goto _test_eof268;
case 268:
	switch( (*( p)) ) {
		case 62: goto st269;
		case 83: goto st270;
		case 115: goto st270;
	}
	goto tr263;
st269:
	if ( ++( p) == ( pe) )
		goto _test_eof269;
case 269:
	switch( (*( p)) ) {
		case 0: goto tr321;
		case 9: goto st269;
		case 10: goto tr321;
		case 32: goto st269;
	}
	goto tr263;
st270:
	if ( ++( p) == ( pe) )
		goto _test_eof270;
case 270:
	if ( (*( p)) == 62 )
		goto st269;
	goto tr263;
tr266:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st271;
st271:
	if ( ++( p) == ( pe) )
		goto _test_eof271;
case 271:
#line 8273 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st272;
		case 72: goto st283;
		case 81: goto st285;
		case 83: goto st290;
		case 104: goto st283;
		case 113: goto st285;
		case 115: goto st290;
	}
	goto tr263;
st272:
	if ( ++( p) == ( pe) )
		goto _test_eof272;
case 272:
	switch( (*( p)) ) {
		case 81: goto st229;
		case 83: goto st273;
		case 84: goto st280;
		case 113: goto st229;
		case 115: goto st273;
		case 116: goto st280;
	}
	goto tr263;
st273:
	if ( ++( p) == ( pe) )
		goto _test_eof273;
case 273:
	switch( (*( p)) ) {
		case 69: goto st274;
		case 101: goto st274;
	}
	goto tr263;
st274:
	if ( ++( p) == ( pe) )
		goto _test_eof274;
case 274:
	switch( (*( p)) ) {
		case 67: goto st275;
		case 99: goto st275;
	}
	goto tr263;
st275:
	if ( ++( p) == ( pe) )
		goto _test_eof275;
case 275:
	switch( (*( p)) ) {
		case 84: goto st276;
		case 116: goto st276;
	}
	goto tr263;
st276:
	if ( ++( p) == ( pe) )
		goto _test_eof276;
case 276:
	switch( (*( p)) ) {
		case 73: goto st277;
		case 105: goto st277;
	}
	goto tr263;
st277:
	if ( ++( p) == ( pe) )
		goto _test_eof277;
case 277:
	switch( (*( p)) ) {
		case 79: goto st278;
		case 111: goto st278;
	}
	goto tr263;
st278:
	if ( ++( p) == ( pe) )
		goto _test_eof278;
case 278:
	switch( (*( p)) ) {
		case 78: goto st279;
		case 110: goto st279;
	}
	goto tr263;
st279:
	if ( ++( p) == ( pe) )
		goto _test_eof279;
case 279:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SECTION)  ) _widec += 256;
	}
	if ( _widec == 3165 )
		goto st1717;
	goto tr263;
st280:
	if ( ++( p) == ( pe) )
		goto _test_eof280;
case 280:
	switch( (*( p)) ) {
		case 68: goto st281;
		case 72: goto st282;
		case 100: goto st281;
		case 104: goto st282;
	}
	goto tr263;
st281:
	if ( ++( p) == ( pe) )
		goto _test_eof281;
case 281:
	if ( (*( p)) == 93 )
		goto tr299;
	goto tr263;
st282:
	if ( ++( p) == ( pe) )
		goto _test_eof282;
case 282:
	if ( (*( p)) == 93 )
		goto tr300;
	goto tr263;
st283:
	if ( ++( p) == ( pe) )
		goto _test_eof283;
case 283:
	switch( (*( p)) ) {
		case 82: goto st284;
		case 114: goto st284;
	}
	goto tr263;
st284:
	if ( ++( p) == ( pe) )
		goto _test_eof284;
case 284:
	if ( (*( p)) == 93 )
		goto st261;
	goto tr263;
st285:
	if ( ++( p) == ( pe) )
		goto _test_eof285;
case 285:
	switch( (*( p)) ) {
		case 85: goto st286;
		case 117: goto st286;
	}
	goto tr263;
st286:
	if ( ++( p) == ( pe) )
		goto _test_eof286;
case 286:
	switch( (*( p)) ) {
		case 79: goto st287;
		case 111: goto st287;
	}
	goto tr263;
st287:
	if ( ++( p) == ( pe) )
		goto _test_eof287;
case 287:
	switch( (*( p)) ) {
		case 84: goto st288;
		case 116: goto st288;
	}
	goto tr263;
st288:
	if ( ++( p) == ( pe) )
		goto _test_eof288;
case 288:
	switch( (*( p)) ) {
		case 69: goto st289;
		case 101: goto st289;
	}
	goto tr263;
st289:
	if ( ++( p) == ( pe) )
		goto _test_eof289;
case 289:
	if ( (*( p)) == 93 )
		goto tr309;
	goto tr263;
st290:
	if ( ++( p) == ( pe) )
		goto _test_eof290;
case 290:
	switch( (*( p)) ) {
		case 80: goto st291;
		case 112: goto st291;
	}
	goto tr263;
st291:
	if ( ++( p) == ( pe) )
		goto _test_eof291;
case 291:
	switch( (*( p)) ) {
		case 79: goto st292;
		case 111: goto st292;
	}
	goto tr263;
st292:
	if ( ++( p) == ( pe) )
		goto _test_eof292;
case 292:
	switch( (*( p)) ) {
		case 73: goto st293;
		case 105: goto st293;
	}
	goto tr263;
st293:
	if ( ++( p) == ( pe) )
		goto _test_eof293;
case 293:
	switch( (*( p)) ) {
		case 76: goto st294;
		case 108: goto st294;
	}
	goto tr263;
st294:
	if ( ++( p) == ( pe) )
		goto _test_eof294;
case 294:
	switch( (*( p)) ) {
		case 69: goto st295;
		case 101: goto st295;
	}
	goto tr263;
st295:
	if ( ++( p) == ( pe) )
		goto _test_eof295;
case 295:
	switch( (*( p)) ) {
		case 82: goto st296;
		case 114: goto st296;
	}
	goto tr263;
st296:
	if ( ++( p) == ( pe) )
		goto _test_eof296;
case 296:
	switch( (*( p)) ) {
		case 83: goto st297;
		case 93: goto st269;
		case 115: goto st297;
	}
	goto tr263;
st297:
	if ( ++( p) == ( pe) )
		goto _test_eof297;
case 297:
	if ( (*( p)) == 93 )
		goto st269;
	goto tr263;
tr2181:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 571 "ext/dtext/dtext.cpp.rl"
	{( act) = 91;}
	goto st1719;
st1719:
	if ( ++( p) == ( pe) )
		goto _test_eof1719;
case 1719:
#line 8530 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto st219;
		case 10: goto tr2181;
		case 32: goto st219;
		case 60: goto st298;
		case 91: goto st302;
	}
	goto tr2190;
st298:
	if ( ++( p) == ( pe) )
		goto _test_eof298;
case 298:
	if ( (*( p)) == 47 )
		goto st299;
	goto tr348;
st299:
	if ( ++( p) == ( pe) )
		goto _test_eof299;
case 299:
	switch( (*( p)) ) {
		case 84: goto st300;
		case 116: goto st300;
	}
	goto tr348;
st300:
	if ( ++( p) == ( pe) )
		goto _test_eof300;
case 300:
	switch( (*( p)) ) {
		case 78: goto st301;
		case 110: goto st301;
	}
	goto tr348;
st301:
	if ( ++( p) == ( pe) )
		goto _test_eof301;
case 301:
	if ( (*( p)) == 62 )
		goto tr352;
	goto tr254;
st302:
	if ( ++( p) == ( pe) )
		goto _test_eof302;
case 302:
	if ( (*( p)) == 47 )
		goto st303;
	goto tr348;
st303:
	if ( ++( p) == ( pe) )
		goto _test_eof303;
case 303:
	switch( (*( p)) ) {
		case 84: goto st304;
		case 116: goto st304;
	}
	goto tr348;
st304:
	if ( ++( p) == ( pe) )
		goto _test_eof304;
case 304:
	switch( (*( p)) ) {
		case 78: goto st305;
		case 110: goto st305;
	}
	goto tr348;
st305:
	if ( ++( p) == ( pe) )
		goto _test_eof305;
case 305:
	if ( (*( p)) == 93 )
		goto tr352;
	goto tr254;
tr2182:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st306;
st306:
	if ( ++( p) == ( pe) )
		goto _test_eof306;
case 306:
#line 8612 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr356;
		case 32: goto tr356;
		case 42: goto st306;
	}
	goto tr263;
tr356:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
	goto st307;
st307:
	if ( ++( p) == ( pe) )
		goto _test_eof307;
case 307:
#line 8627 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr263;
		case 9: goto tr359;
		case 10: goto tr263;
		case 13: goto tr263;
		case 32: goto tr359;
	}
	goto tr358;
tr358:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1720;
st1720:
	if ( ++( p) == ( pe) )
		goto _test_eof1720;
case 1720:
#line 8644 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2193;
		case 10: goto tr2193;
		case 13: goto tr2193;
	}
	goto st1720;
tr359:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1721;
st1721:
	if ( ++( p) == ( pe) )
		goto _test_eof1721;
case 1721:
#line 8659 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2193;
		case 9: goto tr359;
		case 10: goto tr2193;
		case 13: goto tr2193;
		case 32: goto tr359;
	}
	goto tr358;
tr2183:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st308;
st308:
	if ( ++( p) == ( pe) )
		goto _test_eof308;
case 308:
#line 8676 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st309;
		case 66: goto st249;
		case 67: goto st319;
		case 72: goto st259;
		case 78: goto st326;
		case 81: goto st254;
		case 83: goto st333;
		case 84: goto st343;
		case 98: goto st249;
		case 99: goto st319;
		case 104: goto st259;
		case 110: goto st326;
		case 113: goto st254;
		case 115: goto st333;
		case 116: goto st343;
	}
	goto tr263;
st309:
	if ( ++( p) == ( pe) )
		goto _test_eof309;
case 309:
	switch( (*( p)) ) {
		case 66: goto st224;
		case 81: goto st234;
		case 83: goto st310;
		case 84: goto st318;
		case 98: goto st224;
		case 113: goto st234;
		case 115: goto st310;
		case 116: goto st318;
	}
	goto tr263;
st310:
	if ( ++( p) == ( pe) )
		goto _test_eof310;
case 310:
	switch( (*( p)) ) {
		case 69: goto st240;
		case 80: goto st311;
		case 101: goto st240;
		case 112: goto st311;
	}
	goto tr263;
st311:
	if ( ++( p) == ( pe) )
		goto _test_eof311;
case 311:
	switch( (*( p)) ) {
		case 79: goto st312;
		case 111: goto st312;
	}
	goto tr254;
st312:
	if ( ++( p) == ( pe) )
		goto _test_eof312;
case 312:
	switch( (*( p)) ) {
		case 73: goto st313;
		case 105: goto st313;
	}
	goto tr254;
st313:
	if ( ++( p) == ( pe) )
		goto _test_eof313;
case 313:
	switch( (*( p)) ) {
		case 76: goto st314;
		case 108: goto st314;
	}
	goto tr254;
st314:
	if ( ++( p) == ( pe) )
		goto _test_eof314;
case 314:
	switch( (*( p)) ) {
		case 69: goto st315;
		case 101: goto st315;
	}
	goto tr254;
st315:
	if ( ++( p) == ( pe) )
		goto _test_eof315;
case 315:
	switch( (*( p)) ) {
		case 82: goto st316;
		case 114: goto st316;
	}
	goto tr254;
st316:
	if ( ++( p) == ( pe) )
		goto _test_eof316;
case 316:
	switch( (*( p)) ) {
		case 62: goto tr373;
		case 83: goto st317;
		case 115: goto st317;
	}
	goto tr254;
st317:
	if ( ++( p) == ( pe) )
		goto _test_eof317;
case 317:
	if ( (*( p)) == 62 )
		goto tr373;
	goto tr254;
st318:
	if ( ++( p) == ( pe) )
		goto _test_eof318;
case 318:
	switch( (*( p)) ) {
		case 68: goto st247;
		case 72: goto st248;
		case 78: goto st301;
		case 100: goto st247;
		case 104: goto st248;
		case 110: goto st301;
	}
	goto tr254;
st319:
	if ( ++( p) == ( pe) )
		goto _test_eof319;
case 319:
	switch( (*( p)) ) {
		case 79: goto st320;
		case 111: goto st320;
	}
	goto tr263;
st320:
	if ( ++( p) == ( pe) )
		goto _test_eof320;
case 320:
	switch( (*( p)) ) {
		case 68: goto st321;
		case 100: goto st321;
	}
	goto tr263;
st321:
	if ( ++( p) == ( pe) )
		goto _test_eof321;
case 321:
	switch( (*( p)) ) {
		case 69: goto st322;
		case 101: goto st322;
	}
	goto tr263;
st322:
	if ( ++( p) == ( pe) )
		goto _test_eof322;
case 322:
	switch( (*( p)) ) {
		case 9: goto st323;
		case 32: goto st323;
		case 61: goto st324;
		case 62: goto tr380;
	}
	goto tr263;
st323:
	if ( ++( p) == ( pe) )
		goto _test_eof323;
case 323:
	switch( (*( p)) ) {
		case 9: goto st323;
		case 32: goto st323;
		case 61: goto st324;
	}
	goto tr263;
st324:
	if ( ++( p) == ( pe) )
		goto _test_eof324;
case 324:
	switch( (*( p)) ) {
		case 9: goto st324;
		case 32: goto st324;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr381;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr381;
	} else
		goto tr381;
	goto tr263;
tr381:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st325;
st325:
	if ( ++( p) == ( pe) )
		goto _test_eof325;
case 325:
#line 8869 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 62 )
		goto tr383;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st325;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st325;
	} else
		goto st325;
	goto tr263;
st326:
	if ( ++( p) == ( pe) )
		goto _test_eof326;
case 326:
	switch( (*( p)) ) {
		case 79: goto st327;
		case 111: goto st327;
	}
	goto tr263;
st327:
	if ( ++( p) == ( pe) )
		goto _test_eof327;
case 327:
	switch( (*( p)) ) {
		case 68: goto st328;
		case 100: goto st328;
	}
	goto tr263;
st328:
	if ( ++( p) == ( pe) )
		goto _test_eof328;
case 328:
	switch( (*( p)) ) {
		case 84: goto st329;
		case 116: goto st329;
	}
	goto tr263;
st329:
	if ( ++( p) == ( pe) )
		goto _test_eof329;
case 329:
	switch( (*( p)) ) {
		case 69: goto st330;
		case 101: goto st330;
	}
	goto tr263;
st330:
	if ( ++( p) == ( pe) )
		goto _test_eof330;
case 330:
	switch( (*( p)) ) {
		case 88: goto st331;
		case 120: goto st331;
	}
	goto tr263;
st331:
	if ( ++( p) == ( pe) )
		goto _test_eof331;
case 331:
	switch( (*( p)) ) {
		case 84: goto st332;
		case 116: goto st332;
	}
	goto tr263;
st332:
	if ( ++( p) == ( pe) )
		goto _test_eof332;
case 332:
	if ( (*( p)) == 62 )
		goto tr380;
	goto tr263;
st333:
	if ( ++( p) == ( pe) )
		goto _test_eof333;
case 333:
	switch( (*( p)) ) {
		case 69: goto st334;
		case 80: goto st263;
		case 101: goto st334;
		case 112: goto st263;
	}
	goto tr263;
st334:
	if ( ++( p) == ( pe) )
		goto _test_eof334;
case 334:
	switch( (*( p)) ) {
		case 67: goto st335;
		case 99: goto st335;
	}
	goto tr263;
st335:
	if ( ++( p) == ( pe) )
		goto _test_eof335;
case 335:
	switch( (*( p)) ) {
		case 84: goto st336;
		case 116: goto st336;
	}
	goto tr263;
st336:
	if ( ++( p) == ( pe) )
		goto _test_eof336;
case 336:
	switch( (*( p)) ) {
		case 73: goto st337;
		case 105: goto st337;
	}
	goto tr263;
st337:
	if ( ++( p) == ( pe) )
		goto _test_eof337;
case 337:
	switch( (*( p)) ) {
		case 79: goto st338;
		case 111: goto st338;
	}
	goto tr263;
st338:
	if ( ++( p) == ( pe) )
		goto _test_eof338;
case 338:
	switch( (*( p)) ) {
		case 78: goto st339;
		case 110: goto st339;
	}
	goto tr263;
st339:
	if ( ++( p) == ( pe) )
		goto _test_eof339;
case 339:
	switch( (*( p)) ) {
		case 9: goto st340;
		case 32: goto st340;
		case 61: goto st342;
		case 62: goto tr380;
	}
	goto tr263;
tr399:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st340;
st340:
	if ( ++( p) == ( pe) )
		goto _test_eof340;
case 340:
#line 9017 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr263;
		case 9: goto tr399;
		case 10: goto tr263;
		case 13: goto tr263;
		case 32: goto tr399;
		case 61: goto tr400;
		case 62: goto tr401;
	}
	goto tr398;
tr398:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st341;
st341:
	if ( ++( p) == ( pe) )
		goto _test_eof341;
case 341:
#line 9036 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr263;
		case 10: goto tr263;
		case 13: goto tr263;
		case 62: goto tr383;
	}
	goto st341;
tr400:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st342;
st342:
	if ( ++( p) == ( pe) )
		goto _test_eof342;
case 342:
#line 9052 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr263;
		case 9: goto tr400;
		case 10: goto tr263;
		case 13: goto tr263;
		case 32: goto tr400;
		case 62: goto tr401;
	}
	goto tr398;
st343:
	if ( ++( p) == ( pe) )
		goto _test_eof343;
case 343:
	switch( (*( p)) ) {
		case 65: goto st344;
		case 97: goto st344;
	}
	goto tr263;
st344:
	if ( ++( p) == ( pe) )
		goto _test_eof344;
case 344:
	switch( (*( p)) ) {
		case 66: goto st345;
		case 98: goto st345;
	}
	goto tr263;
st345:
	if ( ++( p) == ( pe) )
		goto _test_eof345;
case 345:
	switch( (*( p)) ) {
		case 76: goto st346;
		case 108: goto st346;
	}
	goto tr263;
st346:
	if ( ++( p) == ( pe) )
		goto _test_eof346;
case 346:
	switch( (*( p)) ) {
		case 69: goto st332;
		case 101: goto st332;
	}
	goto tr263;
st347:
	if ( ++( p) == ( pe) )
		goto _test_eof347;
case 347:
	if ( 49 <= (*( p)) && (*( p)) <= 54 )
		goto tr406;
	goto tr263;
tr406:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st348;
st348:
	if ( ++( p) == ( pe) )
		goto _test_eof348;
case 348:
#line 9113 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr407;
		case 46: goto tr408;
	}
	goto tr263;
tr407:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st349;
st349:
	if ( ++( p) == ( pe) )
		goto _test_eof349;
case 349:
#line 9127 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto tr409;
		case 35: goto tr409;
		case 38: goto tr409;
		case 45: goto tr409;
		case 95: goto tr409;
	}
	if ( (*( p)) < 65 ) {
		if ( 47 <= (*( p)) && (*( p)) <= 58 )
			goto tr409;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr409;
	} else
		goto tr409;
	goto tr263;
tr409:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st350;
st350:
	if ( ++( p) == ( pe) )
		goto _test_eof350;
case 350:
#line 9152 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto st350;
		case 35: goto st350;
		case 38: goto st350;
		case 46: goto tr411;
		case 95: goto st350;
	}
	if ( (*( p)) < 65 ) {
		if ( 45 <= (*( p)) && (*( p)) <= 58 )
			goto st350;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st350;
	} else
		goto st350;
	goto tr263;
tr408:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1722;
tr411:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1722;
st1722:
	if ( ++( p) == ( pe) )
		goto _test_eof1722;
case 1722:
#line 9185 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1722;
		case 32: goto st1722;
	}
	goto tr2189;
tr2185:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st351;
st351:
	if ( ++( p) == ( pe) )
		goto _test_eof351;
case 351:
#line 9199 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st352;
		case 67: goto st362;
		case 72: goto st283;
		case 78: goto st369;
		case 81: goto st285;
		case 83: goto st376;
		case 84: goto st386;
		case 99: goto st362;
		case 104: goto st283;
		case 110: goto st369;
		case 113: goto st285;
		case 115: goto st376;
		case 116: goto st386;
	}
	goto tr263;
st352:
	if ( ++( p) == ( pe) )
		goto _test_eof352;
case 352:
	switch( (*( p)) ) {
		case 81: goto st229;
		case 83: goto st353;
		case 84: goto st361;
		case 113: goto st229;
		case 115: goto st353;
		case 116: goto st361;
	}
	goto tr263;
st353:
	if ( ++( p) == ( pe) )
		goto _test_eof353;
case 353:
	switch( (*( p)) ) {
		case 69: goto st274;
		case 80: goto st354;
		case 101: goto st274;
		case 112: goto st354;
	}
	goto tr263;
st354:
	if ( ++( p) == ( pe) )
		goto _test_eof354;
case 354:
	switch( (*( p)) ) {
		case 79: goto st355;
		case 111: goto st355;
	}
	goto tr263;
st355:
	if ( ++( p) == ( pe) )
		goto _test_eof355;
case 355:
	switch( (*( p)) ) {
		case 73: goto st356;
		case 105: goto st356;
	}
	goto tr263;
st356:
	if ( ++( p) == ( pe) )
		goto _test_eof356;
case 356:
	switch( (*( p)) ) {
		case 76: goto st357;
		case 108: goto st357;
	}
	goto tr263;
st357:
	if ( ++( p) == ( pe) )
		goto _test_eof357;
case 357:
	switch( (*( p)) ) {
		case 69: goto st358;
		case 101: goto st358;
	}
	goto tr263;
st358:
	if ( ++( p) == ( pe) )
		goto _test_eof358;
case 358:
	switch( (*( p)) ) {
		case 82: goto st359;
		case 114: goto st359;
	}
	goto tr263;
st359:
	if ( ++( p) == ( pe) )
		goto _test_eof359;
case 359:
	switch( (*( p)) ) {
		case 83: goto st360;
		case 93: goto tr373;
		case 115: goto st360;
	}
	goto tr263;
st360:
	if ( ++( p) == ( pe) )
		goto _test_eof360;
case 360:
	if ( (*( p)) == 93 )
		goto tr373;
	goto tr263;
st361:
	if ( ++( p) == ( pe) )
		goto _test_eof361;
case 361:
	switch( (*( p)) ) {
		case 68: goto st281;
		case 72: goto st282;
		case 78: goto st305;
		case 100: goto st281;
		case 104: goto st282;
		case 110: goto st305;
	}
	goto tr263;
st362:
	if ( ++( p) == ( pe) )
		goto _test_eof362;
case 362:
	switch( (*( p)) ) {
		case 79: goto st363;
		case 111: goto st363;
	}
	goto tr263;
st363:
	if ( ++( p) == ( pe) )
		goto _test_eof363;
case 363:
	switch( (*( p)) ) {
		case 68: goto st364;
		case 100: goto st364;
	}
	goto tr263;
st364:
	if ( ++( p) == ( pe) )
		goto _test_eof364;
case 364:
	switch( (*( p)) ) {
		case 69: goto st365;
		case 101: goto st365;
	}
	goto tr263;
st365:
	if ( ++( p) == ( pe) )
		goto _test_eof365;
case 365:
	switch( (*( p)) ) {
		case 9: goto st366;
		case 32: goto st366;
		case 61: goto st367;
		case 93: goto tr380;
	}
	goto tr263;
st366:
	if ( ++( p) == ( pe) )
		goto _test_eof366;
case 366:
	switch( (*( p)) ) {
		case 9: goto st366;
		case 32: goto st366;
		case 61: goto st367;
	}
	goto tr263;
st367:
	if ( ++( p) == ( pe) )
		goto _test_eof367;
case 367:
	switch( (*( p)) ) {
		case 9: goto st367;
		case 32: goto st367;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr431;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr431;
	} else
		goto tr431;
	goto tr263;
tr431:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st368;
st368:
	if ( ++( p) == ( pe) )
		goto _test_eof368;
case 368:
#line 9388 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto tr383;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st368;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st368;
	} else
		goto st368;
	goto tr263;
st369:
	if ( ++( p) == ( pe) )
		goto _test_eof369;
case 369:
	switch( (*( p)) ) {
		case 79: goto st370;
		case 111: goto st370;
	}
	goto tr263;
st370:
	if ( ++( p) == ( pe) )
		goto _test_eof370;
case 370:
	switch( (*( p)) ) {
		case 68: goto st371;
		case 100: goto st371;
	}
	goto tr263;
st371:
	if ( ++( p) == ( pe) )
		goto _test_eof371;
case 371:
	switch( (*( p)) ) {
		case 84: goto st372;
		case 116: goto st372;
	}
	goto tr263;
st372:
	if ( ++( p) == ( pe) )
		goto _test_eof372;
case 372:
	switch( (*( p)) ) {
		case 69: goto st373;
		case 101: goto st373;
	}
	goto tr263;
st373:
	if ( ++( p) == ( pe) )
		goto _test_eof373;
case 373:
	switch( (*( p)) ) {
		case 88: goto st374;
		case 120: goto st374;
	}
	goto tr263;
st374:
	if ( ++( p) == ( pe) )
		goto _test_eof374;
case 374:
	switch( (*( p)) ) {
		case 84: goto st375;
		case 116: goto st375;
	}
	goto tr263;
st375:
	if ( ++( p) == ( pe) )
		goto _test_eof375;
case 375:
	if ( (*( p)) == 93 )
		goto tr380;
	goto tr263;
st376:
	if ( ++( p) == ( pe) )
		goto _test_eof376;
case 376:
	switch( (*( p)) ) {
		case 69: goto st377;
		case 80: goto st291;
		case 101: goto st377;
		case 112: goto st291;
	}
	goto tr263;
st377:
	if ( ++( p) == ( pe) )
		goto _test_eof377;
case 377:
	switch( (*( p)) ) {
		case 67: goto st378;
		case 99: goto st378;
	}
	goto tr263;
st378:
	if ( ++( p) == ( pe) )
		goto _test_eof378;
case 378:
	switch( (*( p)) ) {
		case 84: goto st379;
		case 116: goto st379;
	}
	goto tr263;
st379:
	if ( ++( p) == ( pe) )
		goto _test_eof379;
case 379:
	switch( (*( p)) ) {
		case 73: goto st380;
		case 105: goto st380;
	}
	goto tr263;
st380:
	if ( ++( p) == ( pe) )
		goto _test_eof380;
case 380:
	switch( (*( p)) ) {
		case 79: goto st381;
		case 111: goto st381;
	}
	goto tr263;
st381:
	if ( ++( p) == ( pe) )
		goto _test_eof381;
case 381:
	switch( (*( p)) ) {
		case 78: goto st382;
		case 110: goto st382;
	}
	goto tr263;
st382:
	if ( ++( p) == ( pe) )
		goto _test_eof382;
case 382:
	switch( (*( p)) ) {
		case 9: goto st383;
		case 32: goto st383;
		case 61: goto st385;
		case 93: goto tr380;
	}
	goto tr263;
tr448:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st383;
st383:
	if ( ++( p) == ( pe) )
		goto _test_eof383;
case 383:
#line 9536 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr263;
		case 9: goto tr448;
		case 10: goto tr263;
		case 13: goto tr263;
		case 32: goto tr448;
		case 61: goto tr449;
		case 93: goto tr401;
	}
	goto tr447;
tr447:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st384;
st384:
	if ( ++( p) == ( pe) )
		goto _test_eof384;
case 384:
#line 9555 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr263;
		case 10: goto tr263;
		case 13: goto tr263;
		case 93: goto tr383;
	}
	goto st384;
tr449:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st385;
st385:
	if ( ++( p) == ( pe) )
		goto _test_eof385;
case 385:
#line 9571 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr263;
		case 9: goto tr449;
		case 10: goto tr263;
		case 13: goto tr263;
		case 32: goto tr449;
		case 93: goto tr401;
	}
	goto tr447;
st386:
	if ( ++( p) == ( pe) )
		goto _test_eof386;
case 386:
	switch( (*( p)) ) {
		case 65: goto st387;
		case 97: goto st387;
	}
	goto tr263;
st387:
	if ( ++( p) == ( pe) )
		goto _test_eof387;
case 387:
	switch( (*( p)) ) {
		case 66: goto st388;
		case 98: goto st388;
	}
	goto tr263;
st388:
	if ( ++( p) == ( pe) )
		goto _test_eof388;
case 388:
	switch( (*( p)) ) {
		case 76: goto st389;
		case 108: goto st389;
	}
	goto tr263;
st389:
	if ( ++( p) == ( pe) )
		goto _test_eof389;
case 389:
	switch( (*( p)) ) {
		case 69: goto st375;
		case 101: goto st375;
	}
	goto tr263;
st390:
	if ( ++( p) == ( pe) )
		goto _test_eof390;
case 390:
	if ( (*( p)) == 96 )
		goto st391;
	goto tr263;
st391:
	if ( ++( p) == ( pe) )
		goto _test_eof391;
case 391:
	if ( (*( p)) == 96 )
		goto st392;
	goto tr263;
tr457:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st392;
st392:
	if ( ++( p) == ( pe) )
		goto _test_eof392;
case 392:
#line 9641 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr456;
		case 9: goto tr457;
		case 10: goto tr456;
		case 32: goto tr457;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr458;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr458;
	} else
		goto tr458;
	goto tr263;
tr466:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st393;
tr456:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st393;
st393:
	if ( ++( p) == ( pe) )
		goto _test_eof393;
case 393:
#line 9671 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr460;
		case 10: goto tr460;
	}
	goto tr459;
tr459:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st394;
st394:
	if ( ++( p) == ( pe) )
		goto _test_eof394;
case 394:
#line 9685 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr462;
		case 10: goto tr462;
	}
	goto st394;
tr462:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st395;
tr460:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st395;
st395:
	if ( ++( p) == ( pe) )
		goto _test_eof395;
case 395:
#line 9705 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr462;
		case 10: goto tr462;
		case 96: goto st396;
	}
	goto st394;
st396:
	if ( ++( p) == ( pe) )
		goto _test_eof396;
case 396:
	switch( (*( p)) ) {
		case 0: goto tr462;
		case 10: goto tr462;
		case 96: goto st397;
	}
	goto st394;
st397:
	if ( ++( p) == ( pe) )
		goto _test_eof397;
case 397:
	switch( (*( p)) ) {
		case 0: goto tr462;
		case 10: goto tr462;
		case 96: goto st398;
	}
	goto st394;
st398:
	if ( ++( p) == ( pe) )
		goto _test_eof398;
case 398:
	switch( (*( p)) ) {
		case 0: goto tr380;
		case 9: goto st398;
		case 10: goto tr380;
		case 32: goto st398;
	}
	goto st394;
tr458:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st399;
st399:
	if ( ++( p) == ( pe) )
		goto _test_eof399;
case 399:
#line 9751 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr466;
		case 9: goto tr467;
		case 10: goto tr466;
		case 32: goto tr467;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st399;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st399;
	} else
		goto st399;
	goto tr263;
tr467:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st400;
st400:
	if ( ++( p) == ( pe) )
		goto _test_eof400;
case 400:
#line 9775 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st393;
		case 9: goto st400;
		case 10: goto st393;
		case 32: goto st400;
	}
	goto tr263;
tr2154:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1723;
st1723:
	if ( ++( p) == ( pe) )
		goto _test_eof1723;
case 1723:
#line 9793 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 34 )
		goto tr2177;
	goto tr2196;
tr2196:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st401;
st401:
	if ( ++( p) == ( pe) )
		goto _test_eof401;
case 401:
#line 9805 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 34 )
		goto tr472;
	goto st401;
tr472:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st402;
st402:
	if ( ++( p) == ( pe) )
		goto _test_eof402;
case 402:
#line 9817 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 58 )
		goto st403;
	goto tr261;
st403:
	if ( ++( p) == ( pe) )
		goto _test_eof403;
case 403:
	switch( (*( p)) ) {
		case 35: goto tr474;
		case 47: goto tr475;
		case 72: goto tr476;
		case 91: goto st462;
		case 104: goto tr476;
	}
	goto tr261;
tr474:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1724;
tr479:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1724;
st1724:
	if ( ++( p) == ( pe) )
		goto _test_eof1724;
case 1724:
#line 9851 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st406;
		case -29: goto st408;
		case -17: goto st410;
		case 32: goto tr2197;
		case 34: goto st414;
		case 35: goto tr2197;
		case 39: goto st414;
		case 44: goto st414;
		case 46: goto st414;
		case 60: goto tr2197;
		case 62: goto tr2197;
		case 63: goto st414;
		case 91: goto tr2197;
		case 93: goto tr2197;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2197;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st405;
		} else
			goto st404;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2197;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st414;
		} else
			goto tr2197;
	} else
		goto st413;
	goto tr479;
st404:
	if ( ++( p) == ( pe) )
		goto _test_eof404;
case 404:
	if ( (*( p)) <= -65 )
		goto tr479;
	goto tr478;
st405:
	if ( ++( p) == ( pe) )
		goto _test_eof405;
case 405:
	if ( (*( p)) <= -65 )
		goto st404;
	goto tr478;
st406:
	if ( ++( p) == ( pe) )
		goto _test_eof406;
case 406:
	if ( (*( p)) == -99 )
		goto st407;
	if ( (*( p)) <= -65 )
		goto st404;
	goto tr478;
st407:
	if ( ++( p) == ( pe) )
		goto _test_eof407;
case 407:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr479;
	} else
		goto tr479;
	goto tr478;
st408:
	if ( ++( p) == ( pe) )
		goto _test_eof408;
case 408:
	if ( (*( p)) == -128 )
		goto st409;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st404;
	goto tr478;
st409:
	if ( ++( p) == ( pe) )
		goto _test_eof409;
case 409:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr479;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr479;
	} else
		goto tr479;
	goto tr478;
st410:
	if ( ++( p) == ( pe) )
		goto _test_eof410;
case 410:
	switch( (*( p)) ) {
		case -68: goto st411;
		case -67: goto st412;
	}
	if ( (*( p)) <= -65 )
		goto st404;
	goto tr478;
st411:
	if ( ++( p) == ( pe) )
		goto _test_eof411;
case 411:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr479;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr479;
	} else
		goto tr479;
	goto tr478;
st412:
	if ( ++( p) == ( pe) )
		goto _test_eof412;
case 412:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr479;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr479;
		} else if ( (*( p)) >= -95 )
			goto tr479;
	} else
		goto tr479;
	goto tr478;
st413:
	if ( ++( p) == ( pe) )
		goto _test_eof413;
case 413:
	if ( (*( p)) <= -65 )
		goto st405;
	goto tr478;
st414:
	if ( ++( p) == ( pe) )
		goto _test_eof414;
case 414:
	switch( (*( p)) ) {
		case -30: goto st406;
		case -29: goto st408;
		case -17: goto st410;
		case 32: goto tr478;
		case 34: goto st414;
		case 35: goto tr478;
		case 39: goto st414;
		case 44: goto st414;
		case 46: goto st414;
		case 60: goto tr478;
		case 62: goto tr478;
		case 63: goto st414;
		case 91: goto tr478;
		case 93: goto tr478;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr478;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st405;
		} else
			goto st404;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr478;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st414;
		} else
			goto tr478;
	} else
		goto st413;
	goto tr479;
tr475:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1725;
tr491:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1725;
st1725:
	if ( ++( p) == ( pe) )
		goto _test_eof1725;
case 1725:
#line 10054 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st417;
		case -29: goto st419;
		case -17: goto st421;
		case 32: goto tr2197;
		case 34: goto st425;
		case 35: goto tr479;
		case 39: goto st425;
		case 44: goto st425;
		case 46: goto st425;
		case 60: goto tr2197;
		case 62: goto tr2197;
		case 63: goto st426;
		case 91: goto tr2197;
		case 93: goto tr2197;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2197;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st416;
		} else
			goto st415;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2197;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st425;
		} else
			goto tr2197;
	} else
		goto st424;
	goto tr491;
st415:
	if ( ++( p) == ( pe) )
		goto _test_eof415;
case 415:
	if ( (*( p)) <= -65 )
		goto tr491;
	goto tr478;
st416:
	if ( ++( p) == ( pe) )
		goto _test_eof416;
case 416:
	if ( (*( p)) <= -65 )
		goto st415;
	goto tr478;
st417:
	if ( ++( p) == ( pe) )
		goto _test_eof417;
case 417:
	if ( (*( p)) == -99 )
		goto st418;
	if ( (*( p)) <= -65 )
		goto st415;
	goto tr478;
st418:
	if ( ++( p) == ( pe) )
		goto _test_eof418;
case 418:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr491;
	} else
		goto tr491;
	goto tr478;
st419:
	if ( ++( p) == ( pe) )
		goto _test_eof419;
case 419:
	if ( (*( p)) == -128 )
		goto st420;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st415;
	goto tr478;
st420:
	if ( ++( p) == ( pe) )
		goto _test_eof420;
case 420:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr491;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr491;
	} else
		goto tr491;
	goto tr478;
st421:
	if ( ++( p) == ( pe) )
		goto _test_eof421;
case 421:
	switch( (*( p)) ) {
		case -68: goto st422;
		case -67: goto st423;
	}
	if ( (*( p)) <= -65 )
		goto st415;
	goto tr478;
st422:
	if ( ++( p) == ( pe) )
		goto _test_eof422;
case 422:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr491;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr491;
	} else
		goto tr491;
	goto tr478;
st423:
	if ( ++( p) == ( pe) )
		goto _test_eof423;
case 423:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr491;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr491;
		} else if ( (*( p)) >= -95 )
			goto tr491;
	} else
		goto tr491;
	goto tr478;
st424:
	if ( ++( p) == ( pe) )
		goto _test_eof424;
case 424:
	if ( (*( p)) <= -65 )
		goto st416;
	goto tr478;
st425:
	if ( ++( p) == ( pe) )
		goto _test_eof425;
case 425:
	switch( (*( p)) ) {
		case -30: goto st417;
		case -29: goto st419;
		case -17: goto st421;
		case 32: goto tr478;
		case 34: goto st425;
		case 35: goto tr479;
		case 39: goto st425;
		case 44: goto st425;
		case 46: goto st425;
		case 60: goto tr478;
		case 62: goto tr478;
		case 63: goto st426;
		case 91: goto tr478;
		case 93: goto tr478;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr478;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st416;
		} else
			goto st415;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr478;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st425;
		} else
			goto tr478;
	} else
		goto st424;
	goto tr491;
st426:
	if ( ++( p) == ( pe) )
		goto _test_eof426;
case 426:
	switch( (*( p)) ) {
		case -30: goto st429;
		case -29: goto st431;
		case -17: goto st433;
		case 32: goto tr254;
		case 34: goto st426;
		case 35: goto tr479;
		case 39: goto st426;
		case 44: goto st426;
		case 46: goto st426;
		case 63: goto st426;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr254;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st428;
		} else
			goto st427;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr254;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st426;
		} else
			goto tr254;
	} else
		goto st436;
	goto tr510;
st427:
	if ( ++( p) == ( pe) )
		goto _test_eof427;
case 427:
	if ( (*( p)) <= -65 )
		goto tr510;
	goto tr254;
tr510:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1726;
st1726:
	if ( ++( p) == ( pe) )
		goto _test_eof1726;
case 1726:
#line 10291 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st429;
		case -29: goto st431;
		case -17: goto st433;
		case 32: goto tr2197;
		case 34: goto st426;
		case 35: goto tr479;
		case 39: goto st426;
		case 44: goto st426;
		case 46: goto st426;
		case 63: goto st426;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2197;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st428;
		} else
			goto st427;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2197;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st426;
		} else
			goto tr2197;
	} else
		goto st436;
	goto tr510;
st428:
	if ( ++( p) == ( pe) )
		goto _test_eof428;
case 428:
	if ( (*( p)) <= -65 )
		goto st427;
	goto tr254;
st429:
	if ( ++( p) == ( pe) )
		goto _test_eof429;
case 429:
	if ( (*( p)) == -99 )
		goto st430;
	if ( (*( p)) <= -65 )
		goto st427;
	goto tr254;
st430:
	if ( ++( p) == ( pe) )
		goto _test_eof430;
case 430:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr510;
	} else
		goto tr510;
	goto tr254;
st431:
	if ( ++( p) == ( pe) )
		goto _test_eof431;
case 431:
	if ( (*( p)) == -128 )
		goto st432;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st427;
	goto tr254;
st432:
	if ( ++( p) == ( pe) )
		goto _test_eof432;
case 432:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr510;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr510;
	} else
		goto tr510;
	goto tr254;
st433:
	if ( ++( p) == ( pe) )
		goto _test_eof433;
case 433:
	switch( (*( p)) ) {
		case -68: goto st434;
		case -67: goto st435;
	}
	if ( (*( p)) <= -65 )
		goto st427;
	goto tr254;
st434:
	if ( ++( p) == ( pe) )
		goto _test_eof434;
case 434:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr510;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr510;
	} else
		goto tr510;
	goto tr254;
st435:
	if ( ++( p) == ( pe) )
		goto _test_eof435;
case 435:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr510;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr510;
		} else if ( (*( p)) >= -95 )
			goto tr510;
	} else
		goto tr510;
	goto tr254;
st436:
	if ( ++( p) == ( pe) )
		goto _test_eof436;
case 436:
	if ( (*( p)) <= -65 )
		goto st428;
	goto tr254;
tr476:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st437;
st437:
	if ( ++( p) == ( pe) )
		goto _test_eof437;
case 437:
#line 10428 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st438;
		case 116: goto st438;
	}
	goto tr261;
st438:
	if ( ++( p) == ( pe) )
		goto _test_eof438;
case 438:
	switch( (*( p)) ) {
		case 84: goto st439;
		case 116: goto st439;
	}
	goto tr261;
st439:
	if ( ++( p) == ( pe) )
		goto _test_eof439;
case 439:
	switch( (*( p)) ) {
		case 80: goto st440;
		case 112: goto st440;
	}
	goto tr261;
st440:
	if ( ++( p) == ( pe) )
		goto _test_eof440;
case 440:
	switch( (*( p)) ) {
		case 58: goto st441;
		case 83: goto st461;
		case 115: goto st461;
	}
	goto tr261;
st441:
	if ( ++( p) == ( pe) )
		goto _test_eof441;
case 441:
	if ( (*( p)) == 47 )
		goto st442;
	goto tr261;
st442:
	if ( ++( p) == ( pe) )
		goto _test_eof442;
case 442:
	if ( (*( p)) == 47 )
		goto st443;
	goto tr261;
st443:
	if ( ++( p) == ( pe) )
		goto _test_eof443;
case 443:
	switch( (*( p)) ) {
		case 45: goto st445;
		case 95: goto st445;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st446;
		} else if ( (*( p)) >= -62 )
			goto st444;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st445;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st445;
		} else
			goto st445;
	} else
		goto st447;
	goto tr261;
st444:
	if ( ++( p) == ( pe) )
		goto _test_eof444;
case 444:
	if ( (*( p)) <= -65 )
		goto st445;
	goto tr261;
st445:
	if ( ++( p) == ( pe) )
		goto _test_eof445;
case 445:
	switch( (*( p)) ) {
		case 45: goto st445;
		case 46: goto st448;
		case 95: goto st445;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st446;
		} else if ( (*( p)) >= -62 )
			goto st444;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st445;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st445;
		} else
			goto st445;
	} else
		goto st447;
	goto tr261;
st446:
	if ( ++( p) == ( pe) )
		goto _test_eof446;
case 446:
	if ( (*( p)) <= -65 )
		goto st444;
	goto tr261;
st447:
	if ( ++( p) == ( pe) )
		goto _test_eof447;
case 447:
	if ( (*( p)) <= -65 )
		goto st446;
	goto tr261;
st448:
	if ( ++( p) == ( pe) )
		goto _test_eof448;
case 448:
	switch( (*( p)) ) {
		case -30: goto st451;
		case -29: goto st454;
		case -17: goto st456;
		case 45: goto tr533;
		case 95: goto tr533;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st450;
		} else if ( (*( p)) >= -62 )
			goto st449;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto tr533;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr533;
		} else
			goto tr533;
	} else
		goto st459;
	goto tr254;
st449:
	if ( ++( p) == ( pe) )
		goto _test_eof449;
case 449:
	if ( (*( p)) <= -65 )
		goto tr533;
	goto tr254;
tr533:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1727;
st1727:
	if ( ++( p) == ( pe) )
		goto _test_eof1727;
case 1727:
#line 10598 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st451;
		case -29: goto st454;
		case -17: goto st456;
		case 35: goto tr479;
		case 46: goto st448;
		case 47: goto tr491;
		case 58: goto st460;
		case 63: goto st426;
		case 95: goto tr533;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st450;
		} else if ( (*( p)) >= -62 )
			goto st449;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr533;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr533;
		} else
			goto tr533;
	} else
		goto st459;
	goto tr2197;
st450:
	if ( ++( p) == ( pe) )
		goto _test_eof450;
case 450:
	if ( (*( p)) <= -65 )
		goto st449;
	goto tr254;
st451:
	if ( ++( p) == ( pe) )
		goto _test_eof451;
case 451:
	if ( (*( p)) == -99 )
		goto st452;
	if ( (*( p)) <= -65 )
		goto st449;
	goto tr254;
st452:
	if ( ++( p) == ( pe) )
		goto _test_eof452;
case 452:
	if ( (*( p)) == -83 )
		goto st453;
	if ( (*( p)) <= -65 )
		goto tr533;
	goto tr254;
st453:
	if ( ++( p) == ( pe) )
		goto _test_eof453;
case 453:
	switch( (*( p)) ) {
		case -30: goto st451;
		case -29: goto st454;
		case -17: goto st456;
		case 35: goto tr479;
		case 46: goto st448;
		case 47: goto tr491;
		case 58: goto st460;
		case 63: goto st426;
		case 95: goto tr533;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st450;
		} else if ( (*( p)) >= -62 )
			goto st449;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr533;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr533;
		} else
			goto tr533;
	} else
		goto st459;
	goto tr254;
st454:
	if ( ++( p) == ( pe) )
		goto _test_eof454;
case 454:
	if ( (*( p)) == -128 )
		goto st455;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st449;
	goto tr254;
st455:
	if ( ++( p) == ( pe) )
		goto _test_eof455;
case 455:
	if ( (*( p)) < -120 ) {
		if ( (*( p)) > -126 ) {
			if ( -125 <= (*( p)) && (*( p)) <= -121 )
				goto tr533;
		} else
			goto st453;
	} else if ( (*( p)) > -111 ) {
		if ( (*( p)) < -108 ) {
			if ( -110 <= (*( p)) && (*( p)) <= -109 )
				goto tr533;
		} else if ( (*( p)) > -100 ) {
			if ( -99 <= (*( p)) && (*( p)) <= -65 )
				goto tr533;
		} else
			goto st453;
	} else
		goto st453;
	goto tr254;
st456:
	if ( ++( p) == ( pe) )
		goto _test_eof456;
case 456:
	switch( (*( p)) ) {
		case -68: goto st457;
		case -67: goto st458;
	}
	if ( (*( p)) <= -65 )
		goto st449;
	goto tr254;
st457:
	if ( ++( p) == ( pe) )
		goto _test_eof457;
case 457:
	switch( (*( p)) ) {
		case -119: goto st453;
		case -67: goto st453;
	}
	if ( (*( p)) <= -65 )
		goto tr533;
	goto tr254;
st458:
	if ( ++( p) == ( pe) )
		goto _test_eof458;
case 458:
	switch( (*( p)) ) {
		case -99: goto st453;
		case -96: goto st453;
		case -93: goto st453;
	}
	if ( (*( p)) <= -65 )
		goto tr533;
	goto tr254;
st459:
	if ( ++( p) == ( pe) )
		goto _test_eof459;
case 459:
	if ( (*( p)) <= -65 )
		goto st450;
	goto tr254;
st460:
	if ( ++( p) == ( pe) )
		goto _test_eof460;
case 460:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr540;
	goto tr254;
tr540:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1728;
st1728:
	if ( ++( p) == ( pe) )
		goto _test_eof1728;
case 1728:
#line 10777 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr479;
		case 47: goto tr491;
		case 63: goto st426;
	}
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr540;
	goto tr2197;
st461:
	if ( ++( p) == ( pe) )
		goto _test_eof461;
case 461:
	if ( (*( p)) == 58 )
		goto st441;
	goto tr261;
st462:
	if ( ++( p) == ( pe) )
		goto _test_eof462;
case 462:
	switch( (*( p)) ) {
		case 35: goto tr541;
		case 47: goto tr541;
		case 72: goto tr542;
		case 104: goto tr542;
	}
	goto tr261;
tr541:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st463;
st463:
	if ( ++( p) == ( pe) )
		goto _test_eof463;
case 463:
#line 10812 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 93: goto tr544;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st463;
tr542:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st464;
st464:
	if ( ++( p) == ( pe) )
		goto _test_eof464;
case 464:
#line 10829 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st465;
		case 116: goto st465;
	}
	goto tr261;
st465:
	if ( ++( p) == ( pe) )
		goto _test_eof465;
case 465:
	switch( (*( p)) ) {
		case 84: goto st466;
		case 116: goto st466;
	}
	goto tr261;
st466:
	if ( ++( p) == ( pe) )
		goto _test_eof466;
case 466:
	switch( (*( p)) ) {
		case 80: goto st467;
		case 112: goto st467;
	}
	goto tr261;
st467:
	if ( ++( p) == ( pe) )
		goto _test_eof467;
case 467:
	switch( (*( p)) ) {
		case 58: goto st468;
		case 83: goto st471;
		case 115: goto st471;
	}
	goto tr261;
st468:
	if ( ++( p) == ( pe) )
		goto _test_eof468;
case 468:
	if ( (*( p)) == 47 )
		goto st469;
	goto tr261;
st469:
	if ( ++( p) == ( pe) )
		goto _test_eof469;
case 469:
	if ( (*( p)) == 47 )
		goto st470;
	goto tr261;
st470:
	if ( ++( p) == ( pe) )
		goto _test_eof470;
case 470:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st463;
st471:
	if ( ++( p) == ( pe) )
		goto _test_eof471;
case 471:
	if ( (*( p)) == 58 )
		goto st468;
	goto tr261;
tr2155:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1729;
st1729:
	if ( ++( p) == ( pe) )
		goto _test_eof1729;
case 1729:
#line 10903 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto st472;
		case 65: goto st475;
		case 67: goto st483;
		case 71: goto st492;
		case 76: goto st498;
		case 78: goto st506;
		case 80: goto st509;
		case 81: goto st515;
		case 97: goto st475;
		case 99: goto st483;
		case 103: goto st492;
		case 108: goto st498;
		case 110: goto st506;
		case 112: goto st509;
		case 113: goto st515;
	}
	goto tr2177;
st472:
	if ( ++( p) == ( pe) )
		goto _test_eof472;
case 472:
	if ( (*( p)) == 51 )
		goto st473;
	goto tr261;
st473:
	if ( ++( p) == ( pe) )
		goto _test_eof473;
case 473:
	if ( (*( p)) == 57 )
		goto st474;
	goto tr261;
st474:
	if ( ++( p) == ( pe) )
		goto _test_eof474;
case 474:
	if ( (*( p)) == 59 )
		goto tr554;
	goto tr261;
st475:
	if ( ++( p) == ( pe) )
		goto _test_eof475;
case 475:
	switch( (*( p)) ) {
		case 77: goto st476;
		case 80: goto st478;
		case 83: goto st481;
		case 109: goto st476;
		case 112: goto st478;
		case 115: goto st481;
	}
	goto tr261;
st476:
	if ( ++( p) == ( pe) )
		goto _test_eof476;
case 476:
	switch( (*( p)) ) {
		case 80: goto st477;
		case 112: goto st477;
	}
	goto tr261;
st477:
	if ( ++( p) == ( pe) )
		goto _test_eof477;
case 477:
	if ( (*( p)) == 59 )
		goto tr559;
	goto tr261;
st478:
	if ( ++( p) == ( pe) )
		goto _test_eof478;
case 478:
	switch( (*( p)) ) {
		case 79: goto st479;
		case 111: goto st479;
	}
	goto tr261;
st479:
	if ( ++( p) == ( pe) )
		goto _test_eof479;
case 479:
	switch( (*( p)) ) {
		case 83: goto st480;
		case 115: goto st480;
	}
	goto tr261;
st480:
	if ( ++( p) == ( pe) )
		goto _test_eof480;
case 480:
	if ( (*( p)) == 59 )
		goto tr562;
	goto tr261;
st481:
	if ( ++( p) == ( pe) )
		goto _test_eof481;
case 481:
	switch( (*( p)) ) {
		case 84: goto st482;
		case 116: goto st482;
	}
	goto tr261;
st482:
	if ( ++( p) == ( pe) )
		goto _test_eof482;
case 482:
	if ( (*( p)) == 59 )
		goto tr564;
	goto tr261;
st483:
	if ( ++( p) == ( pe) )
		goto _test_eof483;
case 483:
	switch( (*( p)) ) {
		case 79: goto st484;
		case 111: goto st484;
	}
	goto tr261;
st484:
	if ( ++( p) == ( pe) )
		goto _test_eof484;
case 484:
	switch( (*( p)) ) {
		case 76: goto st485;
		case 77: goto st488;
		case 108: goto st485;
		case 109: goto st488;
	}
	goto tr261;
st485:
	if ( ++( p) == ( pe) )
		goto _test_eof485;
case 485:
	switch( (*( p)) ) {
		case 79: goto st486;
		case 111: goto st486;
	}
	goto tr261;
st486:
	if ( ++( p) == ( pe) )
		goto _test_eof486;
case 486:
	switch( (*( p)) ) {
		case 78: goto st487;
		case 110: goto st487;
	}
	goto tr261;
st487:
	if ( ++( p) == ( pe) )
		goto _test_eof487;
case 487:
	if ( (*( p)) == 59 )
		goto tr570;
	goto tr261;
st488:
	if ( ++( p) == ( pe) )
		goto _test_eof488;
case 488:
	switch( (*( p)) ) {
		case 77: goto st489;
		case 109: goto st489;
	}
	goto tr261;
st489:
	if ( ++( p) == ( pe) )
		goto _test_eof489;
case 489:
	switch( (*( p)) ) {
		case 65: goto st490;
		case 97: goto st490;
	}
	goto tr261;
st490:
	if ( ++( p) == ( pe) )
		goto _test_eof490;
case 490:
	switch( (*( p)) ) {
		case 84: goto st491;
		case 116: goto st491;
	}
	goto tr261;
st491:
	if ( ++( p) == ( pe) )
		goto _test_eof491;
case 491:
	if ( (*( p)) == 59 )
		goto tr574;
	goto tr261;
st492:
	if ( ++( p) == ( pe) )
		goto _test_eof492;
case 492:
	switch( (*( p)) ) {
		case 82: goto st493;
		case 84: goto st497;
		case 114: goto st493;
		case 116: goto st497;
	}
	goto tr261;
st493:
	if ( ++( p) == ( pe) )
		goto _test_eof493;
case 493:
	switch( (*( p)) ) {
		case 65: goto st494;
		case 97: goto st494;
	}
	goto tr261;
st494:
	if ( ++( p) == ( pe) )
		goto _test_eof494;
case 494:
	switch( (*( p)) ) {
		case 86: goto st495;
		case 118: goto st495;
	}
	goto tr261;
st495:
	if ( ++( p) == ( pe) )
		goto _test_eof495;
case 495:
	switch( (*( p)) ) {
		case 69: goto st496;
		case 101: goto st496;
	}
	goto tr261;
st496:
	if ( ++( p) == ( pe) )
		goto _test_eof496;
case 496:
	if ( (*( p)) == 59 )
		goto tr580;
	goto tr261;
st497:
	if ( ++( p) == ( pe) )
		goto _test_eof497;
case 497:
	if ( (*( p)) == 59 )
		goto tr581;
	goto tr261;
st498:
	if ( ++( p) == ( pe) )
		goto _test_eof498;
case 498:
	switch( (*( p)) ) {
		case 66: goto st499;
		case 84: goto st505;
		case 98: goto st499;
		case 116: goto st505;
	}
	goto tr261;
st499:
	if ( ++( p) == ( pe) )
		goto _test_eof499;
case 499:
	switch( (*( p)) ) {
		case 82: goto st500;
		case 114: goto st500;
	}
	goto tr261;
st500:
	if ( ++( p) == ( pe) )
		goto _test_eof500;
case 500:
	switch( (*( p)) ) {
		case 65: goto st501;
		case 97: goto st501;
	}
	goto tr261;
st501:
	if ( ++( p) == ( pe) )
		goto _test_eof501;
case 501:
	switch( (*( p)) ) {
		case 67: goto st502;
		case 99: goto st502;
	}
	goto tr261;
st502:
	if ( ++( p) == ( pe) )
		goto _test_eof502;
case 502:
	switch( (*( p)) ) {
		case 69: goto st503;
		case 75: goto st504;
		case 101: goto st503;
		case 107: goto st504;
	}
	goto tr261;
st503:
	if ( ++( p) == ( pe) )
		goto _test_eof503;
case 503:
	if ( (*( p)) == 59 )
		goto tr589;
	goto tr261;
st504:
	if ( ++( p) == ( pe) )
		goto _test_eof504;
case 504:
	if ( (*( p)) == 59 )
		goto tr590;
	goto tr261;
st505:
	if ( ++( p) == ( pe) )
		goto _test_eof505;
case 505:
	if ( (*( p)) == 59 )
		goto tr591;
	goto tr261;
st506:
	if ( ++( p) == ( pe) )
		goto _test_eof506;
case 506:
	switch( (*( p)) ) {
		case 85: goto st507;
		case 117: goto st507;
	}
	goto tr261;
st507:
	if ( ++( p) == ( pe) )
		goto _test_eof507;
case 507:
	switch( (*( p)) ) {
		case 77: goto st508;
		case 109: goto st508;
	}
	goto tr261;
st508:
	if ( ++( p) == ( pe) )
		goto _test_eof508;
case 508:
	if ( (*( p)) == 59 )
		goto tr594;
	goto tr261;
st509:
	if ( ++( p) == ( pe) )
		goto _test_eof509;
case 509:
	switch( (*( p)) ) {
		case 69: goto st510;
		case 101: goto st510;
	}
	goto tr261;
st510:
	if ( ++( p) == ( pe) )
		goto _test_eof510;
case 510:
	switch( (*( p)) ) {
		case 82: goto st511;
		case 114: goto st511;
	}
	goto tr261;
st511:
	if ( ++( p) == ( pe) )
		goto _test_eof511;
case 511:
	switch( (*( p)) ) {
		case 73: goto st512;
		case 105: goto st512;
	}
	goto tr261;
st512:
	if ( ++( p) == ( pe) )
		goto _test_eof512;
case 512:
	switch( (*( p)) ) {
		case 79: goto st513;
		case 111: goto st513;
	}
	goto tr261;
st513:
	if ( ++( p) == ( pe) )
		goto _test_eof513;
case 513:
	switch( (*( p)) ) {
		case 68: goto st514;
		case 100: goto st514;
	}
	goto tr261;
st514:
	if ( ++( p) == ( pe) )
		goto _test_eof514;
case 514:
	if ( (*( p)) == 59 )
		goto tr600;
	goto tr261;
st515:
	if ( ++( p) == ( pe) )
		goto _test_eof515;
case 515:
	switch( (*( p)) ) {
		case 85: goto st516;
		case 117: goto st516;
	}
	goto tr261;
st516:
	if ( ++( p) == ( pe) )
		goto _test_eof516;
case 516:
	switch( (*( p)) ) {
		case 79: goto st517;
		case 111: goto st517;
	}
	goto tr261;
st517:
	if ( ++( p) == ( pe) )
		goto _test_eof517;
case 517:
	switch( (*( p)) ) {
		case 84: goto st518;
		case 116: goto st518;
	}
	goto tr261;
st518:
	if ( ++( p) == ( pe) )
		goto _test_eof518;
case 518:
	if ( (*( p)) == 59 )
		goto tr604;
	goto tr261;
tr2206:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1730;
tr2156:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1730;
st1730:
	if ( ++( p) == ( pe) )
		goto _test_eof1730;
case 1730:
#line 11343 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2207:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st519;
st519:
	if ( ++( p) == ( pe) )
		goto _test_eof519;
case 519:
#line 11365 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 91 )
		goto st520;
	goto tr257;
st520:
	if ( ++( p) == ( pe) )
		goto _test_eof520;
case 520:
	switch( (*( p)) ) {
		case 9: goto tr607;
		case 32: goto tr607;
		case 35: goto tr609;
		case 58: goto tr610;
		case 60: goto tr611;
		case 62: goto tr612;
		case 92: goto tr613;
		case 93: goto tr257;
		case 124: goto tr614;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr608;
	goto tr606;
tr606:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st521;
st521:
	if ( ++( p) == ( pe) )
		goto _test_eof521;
case 521:
#line 11395 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr616;
		case 32: goto tr616;
		case 35: goto tr618;
		case 93: goto tr619;
		case 124: goto tr620;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st523;
	goto st521;
tr616:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st522;
st522:
	if ( ++( p) == ( pe) )
		goto _test_eof522;
case 522:
#line 11414 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st522;
		case 32: goto st522;
		case 35: goto st524;
		case 93: goto st527;
		case 124: goto st528;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st523;
	goto st521;
tr608:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st523;
st523:
	if ( ++( p) == ( pe) )
		goto _test_eof523;
case 523:
#line 11433 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st523;
		case 93: goto tr254;
		case 124: goto tr254;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st523;
	goto st521;
tr618:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st524;
st524:
	if ( ++( p) == ( pe) )
		goto _test_eof524;
case 524:
#line 11450 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr616;
		case 32: goto tr616;
		case 35: goto tr618;
		case 93: goto tr619;
		case 124: goto tr620;
	}
	if ( (*( p)) > 13 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr625;
	} else if ( (*( p)) >= 10 )
		goto st523;
	goto st521;
tr625:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st525;
st525:
	if ( ++( p) == ( pe) )
		goto _test_eof525;
case 525:
#line 11472 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr626;
		case 32: goto tr627;
		case 45: goto st533;
		case 93: goto tr630;
		case 95: goto st533;
		case 124: goto tr631;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st525;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st525;
	} else
		goto st525;
	goto tr254;
tr626:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st526;
st526:
	if ( ++( p) == ( pe) )
		goto _test_eof526;
case 526:
#line 11498 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st526;
		case 32: goto st526;
		case 93: goto st527;
		case 124: goto st528;
	}
	goto tr254;
tr619:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st527;
tr630:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st527;
st527:
	if ( ++( p) == ( pe) )
		goto _test_eof527;
case 527:
#line 11518 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1731;
	goto tr254;
st1731:
	if ( ++( p) == ( pe) )
		goto _test_eof1731;
case 1731:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2210;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2210;
	} else
		goto tr2210;
	goto tr2209;
tr2210:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1732;
st1732:
	if ( ++( p) == ( pe) )
		goto _test_eof1732;
case 1732:
#line 11543 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1732;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1732;
	} else
		goto st1732;
	goto tr2211;
tr620:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st528;
tr631:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st528;
tr635:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st528;
st528:
	if ( ++( p) == ( pe) )
		goto _test_eof528;
case 528:
#line 11571 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr635;
		case 32: goto tr635;
		case 93: goto tr636;
		case 124: goto tr254;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto tr634;
tr634:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st529;
st529:
	if ( ++( p) == ( pe) )
		goto _test_eof529;
case 529:
#line 11589 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr638;
		case 32: goto tr638;
		case 93: goto tr639;
		case 124: goto tr254;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st529;
tr638:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st530;
st530:
	if ( ++( p) == ( pe) )
		goto _test_eof530;
case 530:
#line 11607 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st530;
		case 32: goto st530;
		case 93: goto st531;
		case 124: goto tr254;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st529;
tr636:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st531;
tr639:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st531;
st531:
	if ( ++( p) == ( pe) )
		goto _test_eof531;
case 531:
#line 11631 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1733;
	goto tr254;
st1733:
	if ( ++( p) == ( pe) )
		goto _test_eof1733;
case 1733:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2214;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2214;
	} else
		goto tr2214;
	goto tr2213;
tr2214:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1734;
st1734:
	if ( ++( p) == ( pe) )
		goto _test_eof1734;
case 1734:
#line 11656 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1734;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1734;
	} else
		goto st1734;
	goto tr2215;
tr627:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st532;
st532:
	if ( ++( p) == ( pe) )
		goto _test_eof532;
case 532:
#line 11674 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st526;
		case 32: goto st532;
		case 45: goto st533;
		case 93: goto st527;
		case 95: goto st533;
		case 124: goto st528;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st525;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st525;
	} else
		goto st525;
	goto tr254;
st533:
	if ( ++( p) == ( pe) )
		goto _test_eof533;
case 533:
	switch( (*( p)) ) {
		case 32: goto st533;
		case 45: goto st533;
		case 95: goto st533;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st525;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st525;
	} else
		goto st525;
	goto tr254;
tr607:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st534;
st534:
	if ( ++( p) == ( pe) )
		goto _test_eof534;
case 534:
#line 11718 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr607;
		case 32: goto tr607;
		case 58: goto tr610;
		case 60: goto tr611;
		case 62: goto tr612;
		case 92: goto tr613;
		case 93: goto tr257;
		case 124: goto tr614;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr608;
	goto tr606;
tr610:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st535;
st535:
	if ( ++( p) == ( pe) )
		goto _test_eof535;
case 535:
#line 11740 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr616;
		case 32: goto tr616;
		case 35: goto tr618;
		case 93: goto tr619;
		case 124: goto tr644;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st523;
	goto st521;
tr644:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st536;
st536:
	if ( ++( p) == ( pe) )
		goto _test_eof536;
case 536:
#line 11759 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr645;
		case 32: goto tr645;
		case 35: goto tr646;
		case 93: goto tr647;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto tr634;
tr648:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st537;
tr645:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st537;
st537:
	if ( ++( p) == ( pe) )
		goto _test_eof537;
case 537:
#line 11788 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr648;
		case 32: goto tr648;
		case 35: goto tr649;
		case 93: goto tr650;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto tr634;
tr684:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st538;
tr649:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st538;
tr646:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st538;
st538:
	if ( ++( p) == ( pe) )
		goto _test_eof538;
case 538:
#line 11817 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr638;
		case 32: goto tr638;
		case 93: goto tr639;
		case 124: goto tr257;
	}
	if ( (*( p)) > 13 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr651;
	} else if ( (*( p)) >= 10 )
		goto tr257;
	goto st529;
tr651:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st539;
st539:
	if ( ++( p) == ( pe) )
		goto _test_eof539;
case 539:
#line 11838 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr652;
		case 32: goto tr653;
		case 45: goto st543;
		case 93: goto tr656;
		case 95: goto st543;
		case 124: goto tr257;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr257;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st539;
		} else if ( (*( p)) >= 65 )
			goto st539;
	} else
		goto st539;
	goto st529;
tr652:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st540;
st540:
	if ( ++( p) == ( pe) )
		goto _test_eof540;
case 540:
#line 11869 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st540;
		case 32: goto st540;
		case 93: goto st541;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st529;
tr650:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st541;
tr647:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st541;
tr656:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st541;
tr685:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st541;
st541:
	if ( ++( p) == ( pe) )
		goto _test_eof541;
case 541:
#line 11909 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1735;
	goto tr257;
st1735:
	if ( ++( p) == ( pe) )
		goto _test_eof1735;
case 1735:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2217;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2217;
	} else
		goto tr2217;
	goto tr2209;
tr2217:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1736;
st1736:
	if ( ++( p) == ( pe) )
		goto _test_eof1736;
case 1736:
#line 11934 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1736;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1736;
	} else
		goto st1736;
	goto tr2211;
tr653:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st542;
st542:
	if ( ++( p) == ( pe) )
		goto _test_eof542;
case 542:
#line 11954 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st540;
		case 32: goto st542;
		case 45: goto st543;
		case 93: goto st541;
		case 95: goto st543;
		case 124: goto tr257;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr257;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st539;
		} else if ( (*( p)) >= 65 )
			goto st539;
	} else
		goto st539;
	goto st529;
st543:
	if ( ++( p) == ( pe) )
		goto _test_eof543;
case 543:
	switch( (*( p)) ) {
		case 9: goto tr638;
		case 32: goto tr661;
		case 45: goto st543;
		case 93: goto tr639;
		case 95: goto st543;
		case 124: goto tr257;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr257;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st539;
		} else if ( (*( p)) >= 65 )
			goto st539;
	} else
		goto st539;
	goto st529;
tr661:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st544;
st544:
	if ( ++( p) == ( pe) )
		goto _test_eof544;
case 544:
#line 12007 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st530;
		case 32: goto st544;
		case 45: goto st543;
		case 93: goto st531;
		case 95: goto st543;
		case 124: goto tr257;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr257;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st539;
		} else if ( (*( p)) >= 65 )
			goto st539;
	} else
		goto st539;
	goto st529;
tr611:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st545;
st545:
	if ( ++( p) == ( pe) )
		goto _test_eof545;
case 545:
#line 12036 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr616;
		case 32: goto tr616;
		case 35: goto tr618;
		case 93: goto tr619;
		case 124: goto tr663;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st523;
	goto st521;
tr663:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st546;
st546:
	if ( ++( p) == ( pe) )
		goto _test_eof546;
case 546:
#line 12055 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr635;
		case 32: goto tr635;
		case 62: goto tr664;
		case 93: goto tr636;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto tr634;
tr664:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st547;
st547:
	if ( ++( p) == ( pe) )
		goto _test_eof547;
case 547:
#line 12074 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr638;
		case 32: goto tr638;
		case 93: goto tr639;
		case 95: goto st548;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st529;
st548:
	if ( ++( p) == ( pe) )
		goto _test_eof548;
case 548:
	switch( (*( p)) ) {
		case 9: goto tr638;
		case 32: goto tr638;
		case 60: goto st549;
		case 93: goto tr639;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st529;
st549:
	if ( ++( p) == ( pe) )
		goto _test_eof549;
case 549:
	switch( (*( p)) ) {
		case 9: goto tr638;
		case 32: goto tr638;
		case 93: goto tr639;
		case 124: goto st550;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st529;
st550:
	if ( ++( p) == ( pe) )
		goto _test_eof550;
case 550:
	if ( (*( p)) == 62 )
		goto st551;
	goto tr257;
st551:
	if ( ++( p) == ( pe) )
		goto _test_eof551;
case 551:
	switch( (*( p)) ) {
		case 9: goto tr669;
		case 32: goto tr669;
		case 35: goto tr670;
		case 93: goto tr619;
	}
	goto tr257;
tr669:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st552;
st552:
	if ( ++( p) == ( pe) )
		goto _test_eof552;
case 552:
#line 12138 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st552;
		case 32: goto st552;
		case 35: goto st553;
		case 93: goto st527;
	}
	goto tr257;
tr670:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st553;
st553:
	if ( ++( p) == ( pe) )
		goto _test_eof553;
case 553:
#line 12154 "ext/dtext/dtext.cpp"
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr673;
	goto tr257;
tr673:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st554;
st554:
	if ( ++( p) == ( pe) )
		goto _test_eof554;
case 554:
#line 12166 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr674;
		case 32: goto tr675;
		case 45: goto st557;
		case 93: goto tr630;
		case 95: goto st557;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st554;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st554;
	} else
		goto st554;
	goto tr257;
tr674:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st555;
st555:
	if ( ++( p) == ( pe) )
		goto _test_eof555;
case 555:
#line 12191 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st555;
		case 32: goto st555;
		case 93: goto st527;
	}
	goto tr257;
tr675:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st556;
st556:
	if ( ++( p) == ( pe) )
		goto _test_eof556;
case 556:
#line 12206 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st555;
		case 32: goto st556;
		case 45: goto st557;
		case 93: goto st527;
		case 95: goto st557;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st554;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st554;
	} else
		goto st554;
	goto tr257;
st557:
	if ( ++( p) == ( pe) )
		goto _test_eof557;
case 557:
	switch( (*( p)) ) {
		case 32: goto st557;
		case 45: goto st557;
		case 95: goto st557;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st554;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st554;
	} else
		goto st554;
	goto tr257;
tr612:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st558;
st558:
	if ( ++( p) == ( pe) )
		goto _test_eof558;
case 558:
#line 12249 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr616;
		case 32: goto tr616;
		case 35: goto tr618;
		case 58: goto st535;
		case 93: goto tr619;
		case 124: goto tr681;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st523;
	goto st521;
tr681:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st559;
st559:
	if ( ++( p) == ( pe) )
		goto _test_eof559;
case 559:
#line 12269 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr635;
		case 32: goto tr635;
		case 51: goto tr682;
		case 93: goto tr636;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto tr634;
tr682:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st560;
st560:
	if ( ++( p) == ( pe) )
		goto _test_eof560;
case 560:
#line 12288 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr683;
		case 32: goto tr683;
		case 35: goto tr684;
		case 93: goto tr685;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st529;
tr683:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st561;
st561:
	if ( ++( p) == ( pe) )
		goto _test_eof561;
case 561:
#line 12309 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st561;
		case 32: goto st561;
		case 35: goto st538;
		case 93: goto st541;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st529;
tr613:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st562;
st562:
	if ( ++( p) == ( pe) )
		goto _test_eof562;
case 562:
#line 12328 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr616;
		case 32: goto tr616;
		case 35: goto tr618;
		case 93: goto tr619;
		case 124: goto tr688;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st523;
	goto st521;
tr688:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st563;
st563:
	if ( ++( p) == ( pe) )
		goto _test_eof563;
case 563:
#line 12347 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr635;
		case 32: goto tr635;
		case 93: goto tr636;
		case 124: goto st564;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto tr634;
st564:
	if ( ++( p) == ( pe) )
		goto _test_eof564;
case 564:
	if ( (*( p)) == 47 )
		goto st551;
	goto tr257;
tr614:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st565;
st565:
	if ( ++( p) == ( pe) )
		goto _test_eof565;
case 565:
#line 12372 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 95: goto st569;
		case 119: goto st570;
		case 124: goto st571;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st566;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st566;
	} else
		goto st566;
	goto tr257;
st566:
	if ( ++( p) == ( pe) )
		goto _test_eof566;
case 566:
	switch( (*( p)) ) {
		case 9: goto tr694;
		case 32: goto tr694;
		case 35: goto tr695;
		case 93: goto tr619;
		case 124: goto tr620;
	}
	goto tr257;
tr694:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st567;
st567:
	if ( ++( p) == ( pe) )
		goto _test_eof567;
case 567:
#line 12407 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st567;
		case 32: goto st567;
		case 35: goto st568;
		case 93: goto st527;
		case 124: goto st528;
	}
	goto tr257;
tr695:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st568;
st568:
	if ( ++( p) == ( pe) )
		goto _test_eof568;
case 568:
#line 12424 "ext/dtext/dtext.cpp"
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr625;
	goto tr257;
st569:
	if ( ++( p) == ( pe) )
		goto _test_eof569;
case 569:
	if ( (*( p)) == 124 )
		goto st566;
	goto tr257;
st570:
	if ( ++( p) == ( pe) )
		goto _test_eof570;
case 570:
	switch( (*( p)) ) {
		case 9: goto tr694;
		case 32: goto tr694;
		case 35: goto tr695;
		case 93: goto tr619;
		case 124: goto tr644;
	}
	goto tr257;
st571:
	if ( ++( p) == ( pe) )
		goto _test_eof571;
case 571:
	if ( (*( p)) == 95 )
		goto st572;
	goto tr257;
st572:
	if ( ++( p) == ( pe) )
		goto _test_eof572;
case 572:
	if ( (*( p)) == 124 )
		goto st569;
	goto tr257;
tr609:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st573;
st573:
	if ( ++( p) == ( pe) )
		goto _test_eof573;
case 573:
#line 12469 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr616;
		case 32: goto tr616;
		case 35: goto tr618;
		case 45: goto tr699;
		case 93: goto tr619;
		case 95: goto tr699;
		case 124: goto tr620;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto st523;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr699;
		} else if ( (*( p)) >= 65 )
			goto tr699;
	} else
		goto tr699;
	goto st521;
tr699:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st574;
st574:
	if ( ++( p) == ( pe) )
		goto _test_eof574;
case 574:
#line 12499 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr616;
		case 32: goto tr616;
		case 35: goto tr618;
		case 45: goto st574;
		case 93: goto tr701;
		case 95: goto st574;
		case 124: goto tr702;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto st523;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st574;
		} else if ( (*( p)) >= 65 )
			goto st574;
	} else
		goto st574;
	goto st521;
tr701:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st575;
st575:
	if ( ++( p) == ( pe) )
		goto _test_eof575;
case 575:
#line 12529 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1737;
	goto tr257;
st1737:
	if ( ++( p) == ( pe) )
		goto _test_eof1737;
case 1737:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2220;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2220;
	} else
		goto tr2220;
	goto tr2219;
tr2220:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1738;
st1738:
	if ( ++( p) == ( pe) )
		goto _test_eof1738;
case 1738:
#line 12554 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1738;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1738;
	} else
		goto st1738;
	goto tr2221;
tr702:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st576;
tr705:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st576;
st576:
	if ( ++( p) == ( pe) )
		goto _test_eof576;
case 576:
#line 12578 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr705;
		case 32: goto tr705;
		case 93: goto tr706;
		case 124: goto tr257;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto tr704;
tr704:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st577;
st577:
	if ( ++( p) == ( pe) )
		goto _test_eof577;
case 577:
#line 12596 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr708;
		case 32: goto tr708;
		case 93: goto tr709;
		case 124: goto tr254;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st577;
tr708:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st578;
st578:
	if ( ++( p) == ( pe) )
		goto _test_eof578;
case 578:
#line 12614 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st578;
		case 32: goto st578;
		case 93: goto st579;
		case 124: goto tr254;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st577;
tr706:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st579;
tr709:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st579;
st579:
	if ( ++( p) == ( pe) )
		goto _test_eof579;
case 579:
#line 12638 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1739;
	goto tr254;
st1739:
	if ( ++( p) == ( pe) )
		goto _test_eof1739;
case 1739:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2224;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2224;
	} else
		goto tr2224;
	goto tr2223;
tr2224:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1740;
st1740:
	if ( ++( p) == ( pe) )
		goto _test_eof1740;
case 1740:
#line 12663 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1740;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1740;
	} else
		goto st1740;
	goto tr2225;
tr2208:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st580;
st580:
	if ( ++( p) == ( pe) )
		goto _test_eof580;
case 580:
#line 12681 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 123 )
		goto st581;
	goto tr257;
st581:
	if ( ++( p) == ( pe) )
		goto _test_eof581;
case 581:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto st581;
		case 32: goto st581;
		case 45: goto tr715;
		case 58: goto tr716;
		case 60: goto tr717;
		case 62: goto tr718;
		case 92: goto tr719;
		case 124: goto tr720;
		case 126: goto tr715;
	}
	if ( (*( p)) > 13 ) {
		if ( 123 <= (*( p)) && (*( p)) <= 125 )
			goto tr254;
	} else if ( (*( p)) >= 10 )
		goto tr254;
	goto tr714;
tr714:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st582;
st582:
	if ( ++( p) == ( pe) )
		goto _test_eof582;
case 582:
#line 12715 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr722;
		case 32: goto tr722;
		case 123: goto tr254;
		case 124: goto tr723;
		case 125: goto tr724;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st582;
tr722:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st583;
st583:
	if ( ++( p) == ( pe) )
		goto _test_eof583;
case 583:
#line 12735 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto st583;
		case 32: goto st583;
		case 45: goto st584;
		case 58: goto st585;
		case 60: goto st620;
		case 62: goto st621;
		case 92: goto st623;
		case 123: goto tr254;
		case 124: goto st614;
		case 125: goto st592;
		case 126: goto st584;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st582;
tr715:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st584;
st584:
	if ( ++( p) == ( pe) )
		goto _test_eof584;
case 584:
#line 12761 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr722;
		case 32: goto tr722;
		case 58: goto st585;
		case 60: goto st620;
		case 62: goto st621;
		case 92: goto st623;
		case 123: goto tr254;
		case 124: goto tr733;
		case 125: goto tr724;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st582;
tr716:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st585;
st585:
	if ( ++( p) == ( pe) )
		goto _test_eof585;
case 585:
#line 12785 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr722;
		case 32: goto tr722;
		case 123: goto st586;
		case 124: goto tr735;
		case 125: goto tr736;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st582;
st586:
	if ( ++( p) == ( pe) )
		goto _test_eof586;
case 586:
	switch( (*( p)) ) {
		case 9: goto tr722;
		case 32: goto tr722;
		case 124: goto tr723;
		case 125: goto tr724;
	}
	goto tr254;
tr723:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st587;
tr738:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st587;
tr750:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st587;
st587:
	if ( ++( p) == ( pe) )
		goto _test_eof587;
case 587:
#line 12828 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr738;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr738;
		case 125: goto tr740;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr739;
	goto tr737;
tr737:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st588;
st588:
	if ( ++( p) == ( pe) )
		goto _test_eof588;
case 588:
#line 12848 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr742:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st589;
st589:
	if ( ++( p) == ( pe) )
		goto _test_eof589;
case 589:
#line 12868 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto st589;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto st589;
		case 125: goto st591;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr739:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st590;
st590:
	if ( ++( p) == ( pe) )
		goto _test_eof590;
case 590:
#line 12888 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto st590;
		case 125: goto tr254;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr744:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st591;
tr740:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st591;
st591:
	if ( ++( p) == ( pe) )
		goto _test_eof591;
case 591:
#line 12913 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1741;
	goto tr254;
st1741:
	if ( ++( p) == ( pe) )
		goto _test_eof1741;
case 1741:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2228;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2228;
	} else
		goto tr2228;
	goto tr2227;
tr2228:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1742;
st1742:
	if ( ++( p) == ( pe) )
		goto _test_eof1742;
case 1742:
#line 12938 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1742;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1742;
	} else
		goto st1742;
	goto tr2229;
tr724:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st592;
st592:
	if ( ++( p) == ( pe) )
		goto _test_eof592;
case 592:
#line 12956 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1743;
	goto tr254;
tr2237:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1743;
st1743:
	if ( ++( p) == ( pe) )
		goto _test_eof1743;
case 1743:
#line 12970 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2232;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2232;
	} else
		goto tr2232;
	goto tr2231;
tr2232:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1744;
st1744:
	if ( ++( p) == ( pe) )
		goto _test_eof1744;
case 1744:
#line 12988 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1744;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1744;
	} else
		goto st1744;
	goto tr2233;
tr735:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st593;
st593:
	if ( ++( p) == ( pe) )
		goto _test_eof593;
case 593:
#line 13006 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr749;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr749;
		case 124: goto tr750;
		case 125: goto tr751;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr739;
	goto tr737;
tr753:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st594;
tr749:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st594;
st594:
	if ( ++( p) == ( pe) )
		goto _test_eof594;
case 594:
#line 13037 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr753;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr753;
		case 45: goto tr754;
		case 58: goto tr755;
		case 60: goto tr756;
		case 62: goto tr757;
		case 92: goto tr758;
		case 123: goto tr737;
		case 124: goto tr759;
		case 125: goto tr760;
		case 126: goto tr754;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr739;
	goto tr752;
tr752:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st595;
st595:
	if ( ++( p) == ( pe) )
		goto _test_eof595;
case 595:
#line 13065 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 123: goto st588;
		case 124: goto tr723;
		case 125: goto tr763;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st595;
tr762:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st596;
st596:
	if ( ++( p) == ( pe) )
		goto _test_eof596;
case 596:
#line 13089 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto st596;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto st596;
		case 45: goto st597;
		case 58: goto st598;
		case 60: goto st602;
		case 62: goto st608;
		case 92: goto st611;
		case 123: goto st588;
		case 124: goto st614;
		case 125: goto st600;
		case 126: goto st597;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st595;
tr754:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st597;
st597:
	if ( ++( p) == ( pe) )
		goto _test_eof597;
case 597:
#line 13117 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 58: goto st598;
		case 60: goto st602;
		case 62: goto st608;
		case 92: goto st611;
		case 123: goto st588;
		case 124: goto tr733;
		case 125: goto tr763;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st595;
tr755:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st598;
st598:
	if ( ++( p) == ( pe) )
		goto _test_eof598;
case 598:
#line 13143 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 123: goto st599;
		case 124: goto tr735;
		case 125: goto tr772;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st595;
tr782:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st599;
st599:
	if ( ++( p) == ( pe) )
		goto _test_eof599;
case 599:
#line 13165 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 124: goto tr723;
		case 125: goto tr763;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr760:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st600;
tr751:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st600;
tr763:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st600;
st600:
	if ( ++( p) == ( pe) )
		goto _test_eof600;
case 600:
#line 13202 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1745;
	goto tr254;
st1745:
	if ( ++( p) == ( pe) )
		goto _test_eof1745;
case 1745:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2235;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2235;
	} else
		goto tr2235;
	goto tr2231;
tr2235:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1746;
st1746:
	if ( ++( p) == ( pe) )
		goto _test_eof1746;
case 1746:
#line 13227 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1746;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1746;
	} else
		goto st1746;
	goto tr2233;
tr772:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st601;
st601:
	if ( ++( p) == ( pe) )
		goto _test_eof601;
case 601:
#line 13247 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr722;
		case 32: goto tr722;
		case 124: goto tr723;
		case 125: goto tr774;
	}
	goto tr254;
tr774:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1747;
st1747:
	if ( ++( p) == ( pe) )
		goto _test_eof1747;
case 1747:
#line 13263 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto tr2237;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2235;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2235;
	} else
		goto tr2235;
	goto tr2231;
tr756:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st602;
st602:
	if ( ++( p) == ( pe) )
		goto _test_eof602;
case 602:
#line 13283 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 123: goto st588;
		case 124: goto tr775;
		case 125: goto tr763;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st595;
tr775:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st603;
st603:
	if ( ++( p) == ( pe) )
		goto _test_eof603;
case 603:
#line 13305 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr738;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr738;
		case 62: goto tr776;
		case 125: goto tr740;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr739;
	goto tr737;
tr776:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st604;
st604:
	if ( ++( p) == ( pe) )
		goto _test_eof604;
case 604:
#line 13326 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 95: goto st605;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
st605:
	if ( ++( p) == ( pe) )
		goto _test_eof605;
case 605:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 60: goto st606;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
st606:
	if ( ++( p) == ( pe) )
		goto _test_eof606;
case 606:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 124: goto st607;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
st607:
	if ( ++( p) == ( pe) )
		goto _test_eof607;
case 607:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 62: goto st599;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr757:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st608;
st608:
	if ( ++( p) == ( pe) )
		goto _test_eof608;
case 608:
#line 13395 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 58: goto st609;
		case 123: goto st588;
		case 124: goto tr781;
		case 125: goto tr763;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st595;
st609:
	if ( ++( p) == ( pe) )
		goto _test_eof609;
case 609:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 123: goto st588;
		case 124: goto tr735;
		case 125: goto tr763;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st595;
tr781:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st610;
st610:
	if ( ++( p) == ( pe) )
		goto _test_eof610;
case 610:
#line 13435 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr738;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr738;
		case 51: goto tr782;
		case 125: goto tr740;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr739;
	goto tr737;
tr758:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st611;
st611:
	if ( ++( p) == ( pe) )
		goto _test_eof611;
case 611:
#line 13456 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 123: goto st588;
		case 124: goto tr783;
		case 125: goto tr763;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st595;
tr783:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st612;
st612:
	if ( ++( p) == ( pe) )
		goto _test_eof612;
case 612:
#line 13478 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr738;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr738;
		case 124: goto tr784;
		case 125: goto tr740;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr739;
	goto tr737;
tr784:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st613;
st613:
	if ( ++( p) == ( pe) )
		goto _test_eof613;
case 613:
#line 13499 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 47: goto st599;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr733:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st614;
tr759:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st614;
st614:
	if ( ++( p) == ( pe) )
		goto _test_eof614;
case 614:
#line 13524 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr738;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr738;
		case 95: goto tr785;
		case 119: goto tr786;
		case 124: goto tr787;
		case 125: goto tr740;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto tr739;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr782;
		} else if ( (*( p)) >= 65 )
			goto tr782;
	} else
		goto tr782;
	goto tr737;
tr785:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st615;
st615:
	if ( ++( p) == ( pe) )
		goto _test_eof615;
case 615:
#line 13556 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 124: goto st599;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr786:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st616;
st616:
	if ( ++( p) == ( pe) )
		goto _test_eof616;
case 616:
#line 13577 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr762;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr762;
		case 124: goto tr735;
		case 125: goto tr763;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr787:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st617;
st617:
	if ( ++( p) == ( pe) )
		goto _test_eof617;
case 617:
#line 13598 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 95: goto st618;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
st618:
	if ( ++( p) == ( pe) )
		goto _test_eof618;
case 618:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr742;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr742;
		case 124: goto st615;
		case 125: goto tr744;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st590;
	goto st588;
tr736:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st619;
st619:
	if ( ++( p) == ( pe) )
		goto _test_eof619;
case 619:
#line 13635 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr722;
		case 32: goto tr722;
		case 124: goto tr723;
		case 125: goto tr790;
	}
	goto tr254;
tr790:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1748;
st1748:
	if ( ++( p) == ( pe) )
		goto _test_eof1748;
case 1748:
#line 13651 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1743;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2232;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2232;
	} else
		goto tr2232;
	goto tr2231;
tr717:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st620;
st620:
	if ( ++( p) == ( pe) )
		goto _test_eof620;
case 620:
#line 13671 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr722;
		case 32: goto tr722;
		case 123: goto tr254;
		case 124: goto tr775;
		case 125: goto tr724;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st582;
tr718:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st621;
st621:
	if ( ++( p) == ( pe) )
		goto _test_eof621;
case 621:
#line 13691 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr722;
		case 32: goto tr722;
		case 58: goto st622;
		case 123: goto tr254;
		case 124: goto tr781;
		case 125: goto tr724;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st582;
st622:
	if ( ++( p) == ( pe) )
		goto _test_eof622;
case 622:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr722;
		case 32: goto tr722;
		case 123: goto tr254;
		case 124: goto tr735;
		case 125: goto tr724;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st582;
tr719:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st623;
st623:
	if ( ++( p) == ( pe) )
		goto _test_eof623;
case 623:
#line 13727 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr722;
		case 32: goto tr722;
		case 123: goto tr254;
		case 124: goto tr783;
		case 125: goto tr724;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st582;
tr720:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st624;
st624:
	if ( ++( p) == ( pe) )
		goto _test_eof624;
case 624:
#line 13747 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 95: goto st625;
		case 119: goto st626;
		case 124: goto st627;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st586;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st586;
	} else
		goto st586;
	goto tr254;
st625:
	if ( ++( p) == ( pe) )
		goto _test_eof625;
case 625:
	if ( (*( p)) == 124 )
		goto st586;
	goto tr254;
st626:
	if ( ++( p) == ( pe) )
		goto _test_eof626;
case 626:
	switch( (*( p)) ) {
		case 9: goto tr722;
		case 32: goto tr722;
		case 124: goto tr735;
		case 125: goto tr724;
	}
	goto tr254;
st627:
	if ( ++( p) == ( pe) )
		goto _test_eof627;
case 627:
	if ( (*( p)) == 95 )
		goto st628;
	goto tr254;
st628:
	if ( ++( p) == ( pe) )
		goto _test_eof628;
case 628:
	if ( (*( p)) == 124 )
		goto st625;
	goto tr254;
tr2157:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1749;
st1749:
	if ( ++( p) == ( pe) )
		goto _test_eof1749;
case 1749:
#line 13806 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2238;
		case 82: goto tr2239;
		case 86: goto tr2240;
		case 91: goto tr2207;
		case 108: goto tr2238;
		case 114: goto tr2239;
		case 118: goto tr2240;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2238:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1750;
st1750:
	if ( ++( p) == ( pe) )
		goto _test_eof1750;
case 1750:
#line 13836 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2241;
		case 91: goto tr2207;
		case 105: goto tr2241;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2241:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1751;
st1751:
	if ( ++( p) == ( pe) )
		goto _test_eof1751;
case 1751:
#line 13862 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2242;
		case 91: goto tr2207;
		case 97: goto tr2242;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2242:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1752;
st1752:
	if ( ++( p) == ( pe) )
		goto _test_eof1752;
case 1752:
#line 13888 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2243;
		case 91: goto tr2207;
		case 115: goto tr2243;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2243:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1753;
st1753:
	if ( ++( p) == ( pe) )
		goto _test_eof1753;
case 1753:
#line 13914 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st629;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st629:
	if ( ++( p) == ( pe) )
		goto _test_eof629;
case 629:
	if ( (*( p)) == 35 )
		goto st630;
	goto tr257;
st630:
	if ( ++( p) == ( pe) )
		goto _test_eof630;
case 630:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr797;
	goto tr257;
tr797:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1754;
st1754:
	if ( ++( p) == ( pe) )
		goto _test_eof1754;
case 1754:
#line 13951 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1754;
	goto tr2245;
tr2239:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1755;
st1755:
	if ( ++( p) == ( pe) )
		goto _test_eof1755;
case 1755:
#line 13965 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2247;
		case 91: goto tr2207;
		case 116: goto tr2247;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2247:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1756;
st1756:
	if ( ++( p) == ( pe) )
		goto _test_eof1756;
case 1756:
#line 13991 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2248;
		case 91: goto tr2207;
		case 105: goto tr2248;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2248:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1757;
st1757:
	if ( ++( p) == ( pe) )
		goto _test_eof1757;
case 1757:
#line 14017 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2249;
		case 91: goto tr2207;
		case 115: goto tr2249;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2249:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1758;
st1758:
	if ( ++( p) == ( pe) )
		goto _test_eof1758;
case 1758:
#line 14043 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2250;
		case 91: goto tr2207;
		case 116: goto tr2250;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2250:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1759;
st1759:
	if ( ++( p) == ( pe) )
		goto _test_eof1759;
case 1759:
#line 14069 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st631;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st631:
	if ( ++( p) == ( pe) )
		goto _test_eof631;
case 631:
	switch( (*( p)) ) {
		case 35: goto st632;
		case 67: goto st633;
		case 99: goto st633;
	}
	goto tr257;
st632:
	if ( ++( p) == ( pe) )
		goto _test_eof632;
case 632:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr800;
	goto tr257;
tr800:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1760;
st1760:
	if ( ++( p) == ( pe) )
		goto _test_eof1760;
case 1760:
#line 14109 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1760;
	goto tr2252;
st633:
	if ( ++( p) == ( pe) )
		goto _test_eof633;
case 633:
	switch( (*( p)) ) {
		case 72: goto st634;
		case 104: goto st634;
	}
	goto tr257;
st634:
	if ( ++( p) == ( pe) )
		goto _test_eof634;
case 634:
	switch( (*( p)) ) {
		case 65: goto st635;
		case 97: goto st635;
	}
	goto tr257;
st635:
	if ( ++( p) == ( pe) )
		goto _test_eof635;
case 635:
	switch( (*( p)) ) {
		case 78: goto st636;
		case 110: goto st636;
	}
	goto tr257;
st636:
	if ( ++( p) == ( pe) )
		goto _test_eof636;
case 636:
	switch( (*( p)) ) {
		case 71: goto st637;
		case 103: goto st637;
	}
	goto tr257;
st637:
	if ( ++( p) == ( pe) )
		goto _test_eof637;
case 637:
	switch( (*( p)) ) {
		case 69: goto st638;
		case 101: goto st638;
	}
	goto tr257;
st638:
	if ( ++( p) == ( pe) )
		goto _test_eof638;
case 638:
	switch( (*( p)) ) {
		case 83: goto st639;
		case 115: goto st639;
	}
	goto tr257;
st639:
	if ( ++( p) == ( pe) )
		goto _test_eof639;
case 639:
	if ( (*( p)) == 32 )
		goto st640;
	goto tr257;
st640:
	if ( ++( p) == ( pe) )
		goto _test_eof640;
case 640:
	if ( (*( p)) == 35 )
		goto st641;
	goto tr257;
st641:
	if ( ++( p) == ( pe) )
		goto _test_eof641;
case 641:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr809;
	goto tr257;
tr809:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1761;
st1761:
	if ( ++( p) == ( pe) )
		goto _test_eof1761;
case 1761:
#line 14196 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1761;
	goto tr2254;
tr2240:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1762;
st1762:
	if ( ++( p) == ( pe) )
		goto _test_eof1762;
case 1762:
#line 14210 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2256;
		case 91: goto tr2207;
		case 111: goto tr2256;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2256:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1763;
st1763:
	if ( ++( p) == ( pe) )
		goto _test_eof1763;
case 1763:
#line 14236 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2257;
		case 91: goto tr2207;
		case 105: goto tr2257;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2257:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1764;
st1764:
	if ( ++( p) == ( pe) )
		goto _test_eof1764;
case 1764:
#line 14262 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2258;
		case 91: goto tr2207;
		case 100: goto tr2258;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2258:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1765;
st1765:
	if ( ++( p) == ( pe) )
		goto _test_eof1765;
case 1765:
#line 14288 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st642;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st642:
	if ( ++( p) == ( pe) )
		goto _test_eof642;
case 642:
	switch( (*( p)) ) {
		case 80: goto st643;
		case 112: goto st643;
	}
	goto tr257;
st643:
	if ( ++( p) == ( pe) )
		goto _test_eof643;
case 643:
	switch( (*( p)) ) {
		case 79: goto st644;
		case 111: goto st644;
	}
	goto tr257;
st644:
	if ( ++( p) == ( pe) )
		goto _test_eof644;
case 644:
	switch( (*( p)) ) {
		case 83: goto st645;
		case 115: goto st645;
	}
	goto tr257;
st645:
	if ( ++( p) == ( pe) )
		goto _test_eof645;
case 645:
	switch( (*( p)) ) {
		case 84: goto st646;
		case 116: goto st646;
	}
	goto tr257;
st646:
	if ( ++( p) == ( pe) )
		goto _test_eof646;
case 646:
	switch( (*( p)) ) {
		case 73: goto st647;
		case 105: goto st647;
	}
	goto tr257;
st647:
	if ( ++( p) == ( pe) )
		goto _test_eof647;
case 647:
	switch( (*( p)) ) {
		case 78: goto st648;
		case 110: goto st648;
	}
	goto tr257;
st648:
	if ( ++( p) == ( pe) )
		goto _test_eof648;
case 648:
	switch( (*( p)) ) {
		case 71: goto st649;
		case 103: goto st649;
	}
	goto tr257;
st649:
	if ( ++( p) == ( pe) )
		goto _test_eof649;
case 649:
	if ( (*( p)) == 32 )
		goto st650;
	goto tr257;
st650:
	if ( ++( p) == ( pe) )
		goto _test_eof650;
case 650:
	if ( (*( p)) == 35 )
		goto st651;
	goto tr257;
st651:
	if ( ++( p) == ( pe) )
		goto _test_eof651;
case 651:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr819;
	goto tr257;
tr819:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1766;
st1766:
	if ( ++( p) == ( pe) )
		goto _test_eof1766;
case 1766:
#line 14395 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1766;
	goto tr2260;
tr2158:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1767;
st1767:
	if ( ++( p) == ( pe) )
		goto _test_eof1767;
case 1767:
#line 14411 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2262;
		case 85: goto tr2263;
		case 91: goto tr2207;
		case 97: goto tr2262;
		case 117: goto tr2263;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2262:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1768;
st1768:
	if ( ++( p) == ( pe) )
		goto _test_eof1768;
case 1768:
#line 14439 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2264;
		case 91: goto tr2207;
		case 110: goto tr2264;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2264:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1769;
st1769:
	if ( ++( p) == ( pe) )
		goto _test_eof1769;
case 1769:
#line 14465 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st652;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st652:
	if ( ++( p) == ( pe) )
		goto _test_eof652;
case 652:
	if ( (*( p)) == 35 )
		goto st653;
	goto tr257;
st653:
	if ( ++( p) == ( pe) )
		goto _test_eof653;
case 653:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr821;
	goto tr257;
tr821:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1770;
st1770:
	if ( ++( p) == ( pe) )
		goto _test_eof1770;
case 1770:
#line 14502 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1770;
	goto tr2266;
tr2263:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1771;
st1771:
	if ( ++( p) == ( pe) )
		goto _test_eof1771;
case 1771:
#line 14516 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2268;
		case 91: goto tr2207;
		case 114: goto tr2268;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2268:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1772;
st1772:
	if ( ++( p) == ( pe) )
		goto _test_eof1772;
case 1772:
#line 14542 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st654;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st654:
	if ( ++( p) == ( pe) )
		goto _test_eof654;
case 654:
	if ( (*( p)) == 35 )
		goto st655;
	goto tr257;
st655:
	if ( ++( p) == ( pe) )
		goto _test_eof655;
case 655:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr823;
	goto tr257;
tr823:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1773;
st1773:
	if ( ++( p) == ( pe) )
		goto _test_eof1773;
case 1773:
#line 14579 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1773;
	goto tr2270;
tr2159:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1774;
st1774:
	if ( ++( p) == ( pe) )
		goto _test_eof1774;
case 1774:
#line 14595 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2272;
		case 91: goto tr2207;
		case 111: goto tr2272;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2272:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1775;
st1775:
	if ( ++( p) == ( pe) )
		goto _test_eof1775;
case 1775:
#line 14621 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2273;
		case 91: goto tr2207;
		case 109: goto tr2273;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2273:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1776;
st1776:
	if ( ++( p) == ( pe) )
		goto _test_eof1776;
case 1776:
#line 14647 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2274;
		case 91: goto tr2207;
		case 109: goto tr2274;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2274:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1777;
st1777:
	if ( ++( p) == ( pe) )
		goto _test_eof1777;
case 1777:
#line 14673 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2275;
		case 73: goto tr2276;
		case 91: goto tr2207;
		case 101: goto tr2275;
		case 105: goto tr2276;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2275:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1778;
st1778:
	if ( ++( p) == ( pe) )
		goto _test_eof1778;
case 1778:
#line 14701 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2277;
		case 91: goto tr2207;
		case 110: goto tr2277;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2277:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1779;
st1779:
	if ( ++( p) == ( pe) )
		goto _test_eof1779;
case 1779:
#line 14727 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2278;
		case 91: goto tr2207;
		case 116: goto tr2278;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2278:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1780;
st1780:
	if ( ++( p) == ( pe) )
		goto _test_eof1780;
case 1780:
#line 14753 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st656;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st656:
	if ( ++( p) == ( pe) )
		goto _test_eof656;
case 656:
	if ( (*( p)) == 35 )
		goto st657;
	goto tr257;
st657:
	if ( ++( p) == ( pe) )
		goto _test_eof657;
case 657:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr825;
	goto tr257;
tr825:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1781;
st1781:
	if ( ++( p) == ( pe) )
		goto _test_eof1781;
case 1781:
#line 14790 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1781;
	goto tr2280;
tr2276:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1782;
st1782:
	if ( ++( p) == ( pe) )
		goto _test_eof1782;
case 1782:
#line 14804 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2282;
		case 91: goto tr2207;
		case 116: goto tr2282;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2282:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1783;
st1783:
	if ( ++( p) == ( pe) )
		goto _test_eof1783;
case 1783:
#line 14830 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st658;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st658:
	if ( ++( p) == ( pe) )
		goto _test_eof658;
case 658:
	if ( (*( p)) == 35 )
		goto st659;
	goto tr257;
st659:
	if ( ++( p) == ( pe) )
		goto _test_eof659;
case 659:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr827;
	goto tr257;
tr827:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1784;
st1784:
	if ( ++( p) == ( pe) )
		goto _test_eof1784;
case 1784:
#line 14867 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1784;
	goto tr2284;
tr2160:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1785;
st1785:
	if ( ++( p) == ( pe) )
		goto _test_eof1785;
case 1785:
#line 14883 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2286;
		case 78: goto tr2287;
		case 91: goto tr2207;
		case 109: goto tr2286;
		case 110: goto tr2287;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2286:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1786;
st1786:
	if ( ++( p) == ( pe) )
		goto _test_eof1786;
case 1786:
#line 14911 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2288;
		case 91: goto tr2207;
		case 97: goto tr2288;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2288:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1787;
st1787:
	if ( ++( p) == ( pe) )
		goto _test_eof1787;
case 1787:
#line 14937 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2289;
		case 91: goto tr2207;
		case 105: goto tr2289;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2289:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1788;
st1788:
	if ( ++( p) == ( pe) )
		goto _test_eof1788;
case 1788:
#line 14963 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2290;
		case 91: goto tr2207;
		case 108: goto tr2290;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2290:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1789;
st1789:
	if ( ++( p) == ( pe) )
		goto _test_eof1789;
case 1789:
#line 14989 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st660;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st660:
	if ( ++( p) == ( pe) )
		goto _test_eof660;
case 660:
	if ( (*( p)) == 35 )
		goto st661;
	goto tr257;
st661:
	if ( ++( p) == ( pe) )
		goto _test_eof661;
case 661:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr829;
	goto tr257;
tr2294:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1790;
tr829:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1790;
st1790:
	if ( ++( p) == ( pe) )
		goto _test_eof1790;
case 1790:
#line 15032 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto tr2293;
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr2294;
	goto tr2292;
tr2293:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st662;
st662:
	if ( ++( p) == ( pe) )
		goto _test_eof662;
case 662:
#line 15046 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 45: goto tr831;
		case 61: goto tr831;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr831;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr831;
	} else
		goto tr831;
	goto tr830;
tr831:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1791;
st1791:
	if ( ++( p) == ( pe) )
		goto _test_eof1791;
case 1791:
#line 15068 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 45: goto st1791;
		case 61: goto st1791;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1791;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1791;
	} else
		goto st1791;
	goto tr2295;
tr2287:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1792;
st1792:
	if ( ++( p) == ( pe) )
		goto _test_eof1792;
case 1792:
#line 15092 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2297;
		case 91: goto tr2207;
		case 112: goto tr2297;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2297:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1793;
st1793:
	if ( ++( p) == ( pe) )
		goto _test_eof1793;
case 1793:
#line 15118 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st663;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st663:
	if ( ++( p) == ( pe) )
		goto _test_eof663;
case 663:
	if ( (*( p)) == 35 )
		goto st664;
	goto tr257;
st664:
	if ( ++( p) == ( pe) )
		goto _test_eof664;
case 664:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr833;
	goto tr257;
tr833:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1794;
st1794:
	if ( ++( p) == ( pe) )
		goto _test_eof1794;
case 1794:
#line 15155 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1794;
	goto tr2299;
tr2161:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1795;
st1795:
	if ( ++( p) == ( pe) )
		goto _test_eof1795;
case 1795:
#line 15171 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2301;
		case 79: goto tr2302;
		case 91: goto tr2207;
		case 108: goto tr2301;
		case 111: goto tr2302;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2301:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1796;
st1796:
	if ( ++( p) == ( pe) )
		goto _test_eof1796;
case 1796:
#line 15199 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2303;
		case 91: goto tr2207;
		case 97: goto tr2303;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2303:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1797;
st1797:
	if ( ++( p) == ( pe) )
		goto _test_eof1797;
case 1797:
#line 15225 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2304;
		case 91: goto tr2207;
		case 103: goto tr2304;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2304:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1798;
st1798:
	if ( ++( p) == ( pe) )
		goto _test_eof1798;
case 1798:
#line 15251 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st665;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st665:
	if ( ++( p) == ( pe) )
		goto _test_eof665;
case 665:
	if ( (*( p)) == 35 )
		goto st666;
	goto tr257;
st666:
	if ( ++( p) == ( pe) )
		goto _test_eof666;
case 666:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr835;
	goto tr257;
tr835:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1799;
st1799:
	if ( ++( p) == ( pe) )
		goto _test_eof1799;
case 1799:
#line 15288 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1799;
	goto tr2306;
tr2302:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1800;
st1800:
	if ( ++( p) == ( pe) )
		goto _test_eof1800;
case 1800:
#line 15302 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2308;
		case 91: goto tr2207;
		case 114: goto tr2308;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2308:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1801;
st1801:
	if ( ++( p) == ( pe) )
		goto _test_eof1801;
case 1801:
#line 15328 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2309;
		case 91: goto tr2207;
		case 117: goto tr2309;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2309:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1802;
st1802:
	if ( ++( p) == ( pe) )
		goto _test_eof1802;
case 1802:
#line 15354 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2310;
		case 91: goto tr2207;
		case 109: goto tr2310;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2310:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1803;
st1803:
	if ( ++( p) == ( pe) )
		goto _test_eof1803;
case 1803:
#line 15380 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st667;
		case 80: goto tr2312;
		case 91: goto tr2207;
		case 112: goto tr2312;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st667:
	if ( ++( p) == ( pe) )
		goto _test_eof667;
case 667:
	switch( (*( p)) ) {
		case 32: goto st668;
		case 35: goto st669;
		case 80: goto st670;
		case 84: goto st674;
		case 112: goto st670;
		case 116: goto st674;
	}
	goto tr257;
st668:
	if ( ++( p) == ( pe) )
		goto _test_eof668;
case 668:
	if ( (*( p)) == 35 )
		goto st669;
	goto tr257;
st669:
	if ( ++( p) == ( pe) )
		goto _test_eof669;
case 669:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr840;
	goto tr257;
tr840:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1804;
st1804:
	if ( ++( p) == ( pe) )
		goto _test_eof1804;
case 1804:
#line 15432 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1804;
	goto tr2313;
st670:
	if ( ++( p) == ( pe) )
		goto _test_eof670;
case 670:
	switch( (*( p)) ) {
		case 79: goto st671;
		case 111: goto st671;
	}
	goto tr257;
st671:
	if ( ++( p) == ( pe) )
		goto _test_eof671;
case 671:
	switch( (*( p)) ) {
		case 83: goto st672;
		case 115: goto st672;
	}
	goto tr257;
st672:
	if ( ++( p) == ( pe) )
		goto _test_eof672;
case 672:
	switch( (*( p)) ) {
		case 84: goto st673;
		case 116: goto st673;
	}
	goto tr257;
st673:
	if ( ++( p) == ( pe) )
		goto _test_eof673;
case 673:
	if ( (*( p)) == 32 )
		goto st668;
	goto tr257;
st674:
	if ( ++( p) == ( pe) )
		goto _test_eof674;
case 674:
	switch( (*( p)) ) {
		case 79: goto st675;
		case 111: goto st675;
	}
	goto tr257;
st675:
	if ( ++( p) == ( pe) )
		goto _test_eof675;
case 675:
	switch( (*( p)) ) {
		case 80: goto st676;
		case 112: goto st676;
	}
	goto tr257;
st676:
	if ( ++( p) == ( pe) )
		goto _test_eof676;
case 676:
	switch( (*( p)) ) {
		case 73: goto st677;
		case 105: goto st677;
	}
	goto tr257;
st677:
	if ( ++( p) == ( pe) )
		goto _test_eof677;
case 677:
	switch( (*( p)) ) {
		case 67: goto st678;
		case 99: goto st678;
	}
	goto tr257;
st678:
	if ( ++( p) == ( pe) )
		goto _test_eof678;
case 678:
	if ( (*( p)) == 32 )
		goto st679;
	goto tr257;
st679:
	if ( ++( p) == ( pe) )
		goto _test_eof679;
case 679:
	if ( (*( p)) == 35 )
		goto st680;
	goto tr257;
st680:
	if ( ++( p) == ( pe) )
		goto _test_eof680;
case 680:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr850;
	goto tr257;
tr2317:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1805;
tr850:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1805;
st1805:
	if ( ++( p) == ( pe) )
		goto _test_eof1805;
case 1805:
#line 15541 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto tr2316;
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr2317;
	goto tr2315;
tr2316:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st681;
st681:
	if ( ++( p) == ( pe) )
		goto _test_eof681;
case 681:
#line 15555 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto st682;
		case 112: goto st682;
	}
	goto tr851;
st682:
	if ( ++( p) == ( pe) )
		goto _test_eof682;
case 682:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr853;
	goto tr851;
tr853:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1806;
st1806:
	if ( ++( p) == ( pe) )
		goto _test_eof1806;
case 1806:
#line 15576 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1806;
	goto tr2318;
tr2312:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1807;
st1807:
	if ( ++( p) == ( pe) )
		goto _test_eof1807;
case 1807:
#line 15590 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2320;
		case 91: goto tr2207;
		case 111: goto tr2320;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2320:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1808;
st1808:
	if ( ++( p) == ( pe) )
		goto _test_eof1808;
case 1808:
#line 15616 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2321;
		case 91: goto tr2207;
		case 115: goto tr2321;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2321:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1809;
st1809:
	if ( ++( p) == ( pe) )
		goto _test_eof1809;
case 1809:
#line 15642 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2322;
		case 91: goto tr2207;
		case 116: goto tr2322;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2322:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1810;
st1810:
	if ( ++( p) == ( pe) )
		goto _test_eof1810;
case 1810:
#line 15668 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st668;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2162:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1811;
st1811:
	if ( ++( p) == ( pe) )
		goto _test_eof1811;
case 1811:
#line 15695 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2323;
		case 91: goto tr2207;
		case 116: goto tr2323;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2323:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1812;
st1812:
	if ( ++( p) == ( pe) )
		goto _test_eof1812;
case 1812:
#line 15721 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2324;
		case 91: goto tr2207;
		case 116: goto tr2324;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2324:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1813;
st1813:
	if ( ++( p) == ( pe) )
		goto _test_eof1813;
case 1813:
#line 15747 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2325;
		case 91: goto tr2207;
		case 112: goto tr2325;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2325:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1814;
st1814:
	if ( ++( p) == ( pe) )
		goto _test_eof1814;
case 1814:
#line 15773 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 58: goto st683;
		case 83: goto tr2327;
		case 91: goto tr2207;
		case 115: goto tr2327;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st683:
	if ( ++( p) == ( pe) )
		goto _test_eof683;
case 683:
	if ( (*( p)) == 47 )
		goto st684;
	goto tr257;
st684:
	if ( ++( p) == ( pe) )
		goto _test_eof684;
case 684:
	if ( (*( p)) == 47 )
		goto st685;
	goto tr257;
st685:
	if ( ++( p) == ( pe) )
		goto _test_eof685;
case 685:
	switch( (*( p)) ) {
		case 45: goto st687;
		case 95: goto st687;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st688;
		} else if ( (*( p)) >= -62 )
			goto st686;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st687;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st687;
		} else
			goto st687;
	} else
		goto st689;
	goto tr257;
st686:
	if ( ++( p) == ( pe) )
		goto _test_eof686;
case 686:
	if ( (*( p)) <= -65 )
		goto st687;
	goto tr257;
st687:
	if ( ++( p) == ( pe) )
		goto _test_eof687;
case 687:
	switch( (*( p)) ) {
		case 45: goto st687;
		case 46: goto st690;
		case 95: goto st687;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st688;
		} else if ( (*( p)) >= -62 )
			goto st686;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st687;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st687;
		} else
			goto st687;
	} else
		goto st689;
	goto tr257;
st688:
	if ( ++( p) == ( pe) )
		goto _test_eof688;
case 688:
	if ( (*( p)) <= -65 )
		goto st686;
	goto tr257;
st689:
	if ( ++( p) == ( pe) )
		goto _test_eof689;
case 689:
	if ( (*( p)) <= -65 )
		goto st688;
	goto tr257;
st690:
	if ( ++( p) == ( pe) )
		goto _test_eof690;
case 690:
	switch( (*( p)) ) {
		case -30: goto st693;
		case -29: goto st696;
		case -17: goto st698;
		case 45: goto tr867;
		case 95: goto tr867;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st692;
		} else if ( (*( p)) >= -62 )
			goto st691;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto tr867;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr867;
		} else
			goto tr867;
	} else
		goto st701;
	goto tr254;
st691:
	if ( ++( p) == ( pe) )
		goto _test_eof691;
case 691:
	if ( (*( p)) <= -65 )
		goto tr867;
	goto tr254;
tr867:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 410 "ext/dtext/dtext.cpp.rl"
	{( act) = 56;}
	goto st1815;
st1815:
	if ( ++( p) == ( pe) )
		goto _test_eof1815;
case 1815:
#line 15924 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st693;
		case -29: goto st696;
		case -17: goto st698;
		case 35: goto tr870;
		case 46: goto st690;
		case 47: goto tr871;
		case 58: goto st735;
		case 63: goto st724;
		case 95: goto tr867;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st692;
		} else if ( (*( p)) >= -62 )
			goto st691;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr867;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr867;
		} else
			goto tr867;
	} else
		goto st701;
	goto tr2328;
st692:
	if ( ++( p) == ( pe) )
		goto _test_eof692;
case 692:
	if ( (*( p)) <= -65 )
		goto st691;
	goto tr254;
st693:
	if ( ++( p) == ( pe) )
		goto _test_eof693;
case 693:
	if ( (*( p)) == -99 )
		goto st694;
	if ( (*( p)) <= -65 )
		goto st691;
	goto tr254;
st694:
	if ( ++( p) == ( pe) )
		goto _test_eof694;
case 694:
	if ( (*( p)) == -83 )
		goto st695;
	if ( (*( p)) <= -65 )
		goto tr867;
	goto tr254;
st695:
	if ( ++( p) == ( pe) )
		goto _test_eof695;
case 695:
	switch( (*( p)) ) {
		case -30: goto st693;
		case -29: goto st696;
		case -17: goto st698;
		case 35: goto tr870;
		case 46: goto st690;
		case 47: goto tr871;
		case 58: goto st735;
		case 63: goto st724;
		case 95: goto tr867;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st692;
		} else if ( (*( p)) >= -62 )
			goto st691;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr867;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr867;
		} else
			goto tr867;
	} else
		goto st701;
	goto tr254;
st696:
	if ( ++( p) == ( pe) )
		goto _test_eof696;
case 696:
	if ( (*( p)) == -128 )
		goto st697;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st691;
	goto tr254;
st697:
	if ( ++( p) == ( pe) )
		goto _test_eof697;
case 697:
	if ( (*( p)) < -120 ) {
		if ( (*( p)) > -126 ) {
			if ( -125 <= (*( p)) && (*( p)) <= -121 )
				goto tr867;
		} else
			goto st695;
	} else if ( (*( p)) > -111 ) {
		if ( (*( p)) < -108 ) {
			if ( -110 <= (*( p)) && (*( p)) <= -109 )
				goto tr867;
		} else if ( (*( p)) > -100 ) {
			if ( -99 <= (*( p)) && (*( p)) <= -65 )
				goto tr867;
		} else
			goto st695;
	} else
		goto st695;
	goto tr254;
st698:
	if ( ++( p) == ( pe) )
		goto _test_eof698;
case 698:
	switch( (*( p)) ) {
		case -68: goto st699;
		case -67: goto st700;
	}
	if ( (*( p)) <= -65 )
		goto st691;
	goto tr254;
st699:
	if ( ++( p) == ( pe) )
		goto _test_eof699;
case 699:
	switch( (*( p)) ) {
		case -119: goto st695;
		case -67: goto st695;
	}
	if ( (*( p)) <= -65 )
		goto tr867;
	goto tr254;
st700:
	if ( ++( p) == ( pe) )
		goto _test_eof700;
case 700:
	switch( (*( p)) ) {
		case -99: goto st695;
		case -96: goto st695;
		case -93: goto st695;
	}
	if ( (*( p)) <= -65 )
		goto tr867;
	goto tr254;
st701:
	if ( ++( p) == ( pe) )
		goto _test_eof701;
case 701:
	if ( (*( p)) <= -65 )
		goto st692;
	goto tr254;
tr870:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1816;
st1816:
	if ( ++( p) == ( pe) )
		goto _test_eof1816;
case 1816:
#line 16092 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st704;
		case -29: goto st706;
		case -17: goto st708;
		case 32: goto tr2328;
		case 34: goto st712;
		case 35: goto tr2328;
		case 39: goto st712;
		case 44: goto st712;
		case 46: goto st712;
		case 60: goto tr2328;
		case 62: goto tr2328;
		case 63: goto st712;
		case 91: goto tr2328;
		case 93: goto tr2328;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2328;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st703;
		} else
			goto st702;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2328;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st712;
		} else
			goto tr2328;
	} else
		goto st711;
	goto tr870;
st702:
	if ( ++( p) == ( pe) )
		goto _test_eof702;
case 702:
	if ( (*( p)) <= -65 )
		goto tr870;
	goto tr877;
st703:
	if ( ++( p) == ( pe) )
		goto _test_eof703;
case 703:
	if ( (*( p)) <= -65 )
		goto st702;
	goto tr877;
st704:
	if ( ++( p) == ( pe) )
		goto _test_eof704;
case 704:
	if ( (*( p)) == -99 )
		goto st705;
	if ( (*( p)) <= -65 )
		goto st702;
	goto tr877;
st705:
	if ( ++( p) == ( pe) )
		goto _test_eof705;
case 705:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr870;
	} else
		goto tr870;
	goto tr877;
st706:
	if ( ++( p) == ( pe) )
		goto _test_eof706;
case 706:
	if ( (*( p)) == -128 )
		goto st707;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st702;
	goto tr877;
st707:
	if ( ++( p) == ( pe) )
		goto _test_eof707;
case 707:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr870;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr870;
	} else
		goto tr870;
	goto tr877;
st708:
	if ( ++( p) == ( pe) )
		goto _test_eof708;
case 708:
	switch( (*( p)) ) {
		case -68: goto st709;
		case -67: goto st710;
	}
	if ( (*( p)) <= -65 )
		goto st702;
	goto tr877;
st709:
	if ( ++( p) == ( pe) )
		goto _test_eof709;
case 709:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr870;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr870;
	} else
		goto tr870;
	goto tr877;
st710:
	if ( ++( p) == ( pe) )
		goto _test_eof710;
case 710:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr870;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr870;
		} else if ( (*( p)) >= -95 )
			goto tr870;
	} else
		goto tr870;
	goto tr877;
st711:
	if ( ++( p) == ( pe) )
		goto _test_eof711;
case 711:
	if ( (*( p)) <= -65 )
		goto st703;
	goto tr877;
st712:
	if ( ++( p) == ( pe) )
		goto _test_eof712;
case 712:
	switch( (*( p)) ) {
		case -30: goto st704;
		case -29: goto st706;
		case -17: goto st708;
		case 32: goto tr877;
		case 34: goto st712;
		case 35: goto tr877;
		case 39: goto st712;
		case 44: goto st712;
		case 46: goto st712;
		case 60: goto tr877;
		case 62: goto tr877;
		case 63: goto st712;
		case 91: goto tr877;
		case 93: goto tr877;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr877;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st703;
		} else
			goto st702;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr877;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st712;
		} else
			goto tr877;
	} else
		goto st711;
	goto tr870;
tr871:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 410 "ext/dtext/dtext.cpp.rl"
	{( act) = 56;}
	goto st1817;
st1817:
	if ( ++( p) == ( pe) )
		goto _test_eof1817;
case 1817:
#line 16283 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st715;
		case -29: goto st717;
		case -17: goto st719;
		case 32: goto tr2328;
		case 34: goto st723;
		case 35: goto tr870;
		case 39: goto st723;
		case 44: goto st723;
		case 46: goto st723;
		case 60: goto tr2328;
		case 62: goto tr2328;
		case 63: goto st724;
		case 91: goto tr2328;
		case 93: goto tr2328;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2328;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st714;
		} else
			goto st713;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2328;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st723;
		} else
			goto tr2328;
	} else
		goto st722;
	goto tr871;
st713:
	if ( ++( p) == ( pe) )
		goto _test_eof713;
case 713:
	if ( (*( p)) <= -65 )
		goto tr871;
	goto tr877;
st714:
	if ( ++( p) == ( pe) )
		goto _test_eof714;
case 714:
	if ( (*( p)) <= -65 )
		goto st713;
	goto tr877;
st715:
	if ( ++( p) == ( pe) )
		goto _test_eof715;
case 715:
	if ( (*( p)) == -99 )
		goto st716;
	if ( (*( p)) <= -65 )
		goto st713;
	goto tr877;
st716:
	if ( ++( p) == ( pe) )
		goto _test_eof716;
case 716:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr871;
	} else
		goto tr871;
	goto tr877;
st717:
	if ( ++( p) == ( pe) )
		goto _test_eof717;
case 717:
	if ( (*( p)) == -128 )
		goto st718;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st713;
	goto tr877;
st718:
	if ( ++( p) == ( pe) )
		goto _test_eof718;
case 718:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr871;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr871;
	} else
		goto tr871;
	goto tr877;
st719:
	if ( ++( p) == ( pe) )
		goto _test_eof719;
case 719:
	switch( (*( p)) ) {
		case -68: goto st720;
		case -67: goto st721;
	}
	if ( (*( p)) <= -65 )
		goto st713;
	goto tr877;
st720:
	if ( ++( p) == ( pe) )
		goto _test_eof720;
case 720:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr871;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr871;
	} else
		goto tr871;
	goto tr877;
st721:
	if ( ++( p) == ( pe) )
		goto _test_eof721;
case 721:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr871;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr871;
		} else if ( (*( p)) >= -95 )
			goto tr871;
	} else
		goto tr871;
	goto tr877;
st722:
	if ( ++( p) == ( pe) )
		goto _test_eof722;
case 722:
	if ( (*( p)) <= -65 )
		goto st714;
	goto tr877;
st723:
	if ( ++( p) == ( pe) )
		goto _test_eof723;
case 723:
	switch( (*( p)) ) {
		case -30: goto st715;
		case -29: goto st717;
		case -17: goto st719;
		case 32: goto tr877;
		case 34: goto st723;
		case 35: goto tr870;
		case 39: goto st723;
		case 44: goto st723;
		case 46: goto st723;
		case 60: goto tr877;
		case 62: goto tr877;
		case 63: goto st724;
		case 91: goto tr877;
		case 93: goto tr877;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr877;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st714;
		} else
			goto st713;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr877;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st723;
		} else
			goto tr877;
	} else
		goto st722;
	goto tr871;
st724:
	if ( ++( p) == ( pe) )
		goto _test_eof724;
case 724:
	switch( (*( p)) ) {
		case -30: goto st727;
		case -29: goto st729;
		case -17: goto st731;
		case 32: goto tr254;
		case 34: goto st724;
		case 35: goto tr870;
		case 39: goto st724;
		case 44: goto st724;
		case 46: goto st724;
		case 63: goto st724;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr254;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st726;
		} else
			goto st725;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr254;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st724;
		} else
			goto tr254;
	} else
		goto st734;
	goto tr906;
st725:
	if ( ++( p) == ( pe) )
		goto _test_eof725;
case 725:
	if ( (*( p)) <= -65 )
		goto tr906;
	goto tr254;
tr906:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 410 "ext/dtext/dtext.cpp.rl"
	{( act) = 56;}
	goto st1818;
st1818:
	if ( ++( p) == ( pe) )
		goto _test_eof1818;
case 1818:
#line 16518 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st727;
		case -29: goto st729;
		case -17: goto st731;
		case 32: goto tr2328;
		case 34: goto st724;
		case 35: goto tr870;
		case 39: goto st724;
		case 44: goto st724;
		case 46: goto st724;
		case 63: goto st724;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2328;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st726;
		} else
			goto st725;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2328;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st724;
		} else
			goto tr2328;
	} else
		goto st734;
	goto tr906;
st726:
	if ( ++( p) == ( pe) )
		goto _test_eof726;
case 726:
	if ( (*( p)) <= -65 )
		goto st725;
	goto tr254;
st727:
	if ( ++( p) == ( pe) )
		goto _test_eof727;
case 727:
	if ( (*( p)) == -99 )
		goto st728;
	if ( (*( p)) <= -65 )
		goto st725;
	goto tr254;
st728:
	if ( ++( p) == ( pe) )
		goto _test_eof728;
case 728:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr906;
	} else
		goto tr906;
	goto tr254;
st729:
	if ( ++( p) == ( pe) )
		goto _test_eof729;
case 729:
	if ( (*( p)) == -128 )
		goto st730;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st725;
	goto tr254;
st730:
	if ( ++( p) == ( pe) )
		goto _test_eof730;
case 730:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr906;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr906;
	} else
		goto tr906;
	goto tr254;
st731:
	if ( ++( p) == ( pe) )
		goto _test_eof731;
case 731:
	switch( (*( p)) ) {
		case -68: goto st732;
		case -67: goto st733;
	}
	if ( (*( p)) <= -65 )
		goto st725;
	goto tr254;
st732:
	if ( ++( p) == ( pe) )
		goto _test_eof732;
case 732:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr906;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr906;
	} else
		goto tr906;
	goto tr254;
st733:
	if ( ++( p) == ( pe) )
		goto _test_eof733;
case 733:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr906;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr906;
		} else if ( (*( p)) >= -95 )
			goto tr906;
	} else
		goto tr906;
	goto tr254;
st734:
	if ( ++( p) == ( pe) )
		goto _test_eof734;
case 734:
	if ( (*( p)) <= -65 )
		goto st726;
	goto tr254;
st735:
	if ( ++( p) == ( pe) )
		goto _test_eof735;
case 735:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr911;
	goto tr254;
tr911:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 410 "ext/dtext/dtext.cpp.rl"
	{( act) = 56;}
	goto st1819;
st1819:
	if ( ++( p) == ( pe) )
		goto _test_eof1819;
case 1819:
#line 16664 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr870;
		case 47: goto tr871;
		case 63: goto st724;
	}
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr911;
	goto tr2328;
tr2327:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1820;
st1820:
	if ( ++( p) == ( pe) )
		goto _test_eof1820;
case 1820:
#line 16683 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 58: goto st683;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2163:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1821;
st1821:
	if ( ++( p) == ( pe) )
		goto _test_eof1821;
case 1821:
#line 16710 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2329;
		case 83: goto tr2330;
		case 91: goto tr2207;
		case 109: goto tr2329;
		case 115: goto tr2330;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2329:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1822;
st1822:
	if ( ++( p) == ( pe) )
		goto _test_eof1822;
case 1822:
#line 16738 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2331;
		case 91: goto tr2207;
		case 112: goto tr2331;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2331:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1823;
st1823:
	if ( ++( p) == ( pe) )
		goto _test_eof1823;
case 1823:
#line 16764 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2332;
		case 91: goto tr2207;
		case 108: goto tr2332;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2332:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1824;
st1824:
	if ( ++( p) == ( pe) )
		goto _test_eof1824;
case 1824:
#line 16790 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2333;
		case 91: goto tr2207;
		case 105: goto tr2333;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2333:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1825;
st1825:
	if ( ++( p) == ( pe) )
		goto _test_eof1825;
case 1825:
#line 16816 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2334;
		case 91: goto tr2207;
		case 99: goto tr2334;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2334:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1826;
st1826:
	if ( ++( p) == ( pe) )
		goto _test_eof1826;
case 1826:
#line 16842 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2335;
		case 91: goto tr2207;
		case 97: goto tr2335;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2335:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1827;
st1827:
	if ( ++( p) == ( pe) )
		goto _test_eof1827;
case 1827:
#line 16868 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2336;
		case 91: goto tr2207;
		case 116: goto tr2336;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2336:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1828;
st1828:
	if ( ++( p) == ( pe) )
		goto _test_eof1828;
case 1828:
#line 16894 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2337;
		case 91: goto tr2207;
		case 105: goto tr2337;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2337:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1829;
st1829:
	if ( ++( p) == ( pe) )
		goto _test_eof1829;
case 1829:
#line 16920 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2338;
		case 91: goto tr2207;
		case 111: goto tr2338;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2338:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1830;
st1830:
	if ( ++( p) == ( pe) )
		goto _test_eof1830;
case 1830:
#line 16946 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2339;
		case 91: goto tr2207;
		case 110: goto tr2339;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2339:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1831;
st1831:
	if ( ++( p) == ( pe) )
		goto _test_eof1831;
case 1831:
#line 16972 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st736;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st736:
	if ( ++( p) == ( pe) )
		goto _test_eof736;
case 736:
	if ( (*( p)) == 35 )
		goto st737;
	goto tr257;
st737:
	if ( ++( p) == ( pe) )
		goto _test_eof737;
case 737:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr913;
	goto tr257;
tr913:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1832;
st1832:
	if ( ++( p) == ( pe) )
		goto _test_eof1832;
case 1832:
#line 17009 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1832;
	goto tr2341;
tr2330:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1833;
st1833:
	if ( ++( p) == ( pe) )
		goto _test_eof1833;
case 1833:
#line 17023 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2343;
		case 91: goto tr2207;
		case 115: goto tr2343;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2343:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1834;
st1834:
	if ( ++( p) == ( pe) )
		goto _test_eof1834;
case 1834:
#line 17049 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2344;
		case 91: goto tr2207;
		case 117: goto tr2344;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2344:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1835;
st1835:
	if ( ++( p) == ( pe) )
		goto _test_eof1835;
case 1835:
#line 17075 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2345;
		case 91: goto tr2207;
		case 101: goto tr2345;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2345:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1836;
st1836:
	if ( ++( p) == ( pe) )
		goto _test_eof1836;
case 1836:
#line 17101 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st738;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st738:
	if ( ++( p) == ( pe) )
		goto _test_eof738;
case 738:
	if ( (*( p)) == 35 )
		goto st739;
	goto tr257;
st739:
	if ( ++( p) == ( pe) )
		goto _test_eof739;
case 739:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr915;
	goto tr257;
tr915:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1837;
st1837:
	if ( ++( p) == ( pe) )
		goto _test_eof1837;
case 1837:
#line 17138 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1837;
	goto tr2347;
tr2164:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1838;
st1838:
	if ( ++( p) == ( pe) )
		goto _test_eof1838;
case 1838:
#line 17154 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2349;
		case 91: goto tr2207;
		case 111: goto tr2349;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2349:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1839;
st1839:
	if ( ++( p) == ( pe) )
		goto _test_eof1839;
case 1839:
#line 17180 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2350;
		case 91: goto tr2207;
		case 100: goto tr2350;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2350:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1840;
st1840:
	if ( ++( p) == ( pe) )
		goto _test_eof1840;
case 1840:
#line 17206 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st740;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st740:
	if ( ++( p) == ( pe) )
		goto _test_eof740;
case 740:
	switch( (*( p)) ) {
		case 65: goto st741;
		case 97: goto st741;
	}
	goto tr257;
st741:
	if ( ++( p) == ( pe) )
		goto _test_eof741;
case 741:
	switch( (*( p)) ) {
		case 67: goto st742;
		case 99: goto st742;
	}
	goto tr257;
st742:
	if ( ++( p) == ( pe) )
		goto _test_eof742;
case 742:
	switch( (*( p)) ) {
		case 84: goto st743;
		case 116: goto st743;
	}
	goto tr257;
st743:
	if ( ++( p) == ( pe) )
		goto _test_eof743;
case 743:
	switch( (*( p)) ) {
		case 73: goto st744;
		case 105: goto st744;
	}
	goto tr257;
st744:
	if ( ++( p) == ( pe) )
		goto _test_eof744;
case 744:
	switch( (*( p)) ) {
		case 79: goto st745;
		case 111: goto st745;
	}
	goto tr257;
st745:
	if ( ++( p) == ( pe) )
		goto _test_eof745;
case 745:
	switch( (*( p)) ) {
		case 78: goto st746;
		case 110: goto st746;
	}
	goto tr257;
st746:
	if ( ++( p) == ( pe) )
		goto _test_eof746;
case 746:
	if ( (*( p)) == 32 )
		goto st747;
	goto tr257;
st747:
	if ( ++( p) == ( pe) )
		goto _test_eof747;
case 747:
	if ( (*( p)) == 35 )
		goto st748;
	goto tr257;
st748:
	if ( ++( p) == ( pe) )
		goto _test_eof748;
case 748:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr924;
	goto tr257;
tr924:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1841;
st1841:
	if ( ++( p) == ( pe) )
		goto _test_eof1841;
case 1841:
#line 17304 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1841;
	goto tr2352;
tr2165:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1842;
st1842:
	if ( ++( p) == ( pe) )
		goto _test_eof1842;
case 1842:
#line 17320 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2354;
		case 91: goto tr2207;
		case 111: goto tr2354;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2354:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1843;
st1843:
	if ( ++( p) == ( pe) )
		goto _test_eof1843;
case 1843:
#line 17346 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2355;
		case 91: goto tr2207;
		case 116: goto tr2355;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2355:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1844;
st1844:
	if ( ++( p) == ( pe) )
		goto _test_eof1844;
case 1844:
#line 17372 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2356;
		case 91: goto tr2207;
		case 101: goto tr2356;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2356:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1845;
st1845:
	if ( ++( p) == ( pe) )
		goto _test_eof1845;
case 1845:
#line 17398 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st749;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st749:
	if ( ++( p) == ( pe) )
		goto _test_eof749;
case 749:
	if ( (*( p)) == 35 )
		goto st750;
	goto tr257;
st750:
	if ( ++( p) == ( pe) )
		goto _test_eof750;
case 750:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr926;
	goto tr257;
tr926:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1846;
st1846:
	if ( ++( p) == ( pe) )
		goto _test_eof1846;
case 1846:
#line 17435 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1846;
	goto tr2358;
tr2166:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1847;
st1847:
	if ( ++( p) == ( pe) )
		goto _test_eof1847;
case 1847:
#line 17451 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2360;
		case 85: goto tr2361;
		case 91: goto tr2207;
		case 111: goto tr2360;
		case 117: goto tr2361;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2360:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1848;
st1848:
	if ( ++( p) == ( pe) )
		goto _test_eof1848;
case 1848:
#line 17479 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2362;
		case 83: goto tr2363;
		case 91: goto tr2207;
		case 111: goto tr2362;
		case 115: goto tr2363;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2362:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1849;
st1849:
	if ( ++( p) == ( pe) )
		goto _test_eof1849;
case 1849:
#line 17507 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2364;
		case 91: goto tr2207;
		case 108: goto tr2364;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2364:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1850;
st1850:
	if ( ++( p) == ( pe) )
		goto _test_eof1850;
case 1850:
#line 17533 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st751;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st751:
	if ( ++( p) == ( pe) )
		goto _test_eof751;
case 751:
	if ( (*( p)) == 35 )
		goto st752;
	goto tr257;
st752:
	if ( ++( p) == ( pe) )
		goto _test_eof752;
case 752:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr928;
	goto tr257;
tr928:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1851;
st1851:
	if ( ++( p) == ( pe) )
		goto _test_eof1851;
case 1851:
#line 17570 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1851;
	goto tr2366;
tr2363:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1852;
st1852:
	if ( ++( p) == ( pe) )
		goto _test_eof1852;
case 1852:
#line 17584 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2368;
		case 91: goto tr2207;
		case 116: goto tr2368;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2368:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1853;
st1853:
	if ( ++( p) == ( pe) )
		goto _test_eof1853;
case 1853:
#line 17610 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st753;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st753:
	if ( ++( p) == ( pe) )
		goto _test_eof753;
case 753:
	switch( (*( p)) ) {
		case 35: goto st754;
		case 67: goto st755;
		case 99: goto st755;
	}
	goto tr257;
st754:
	if ( ++( p) == ( pe) )
		goto _test_eof754;
case 754:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr931;
	goto tr257;
tr931:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1854;
st1854:
	if ( ++( p) == ( pe) )
		goto _test_eof1854;
case 1854:
#line 17650 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1854;
	goto tr2370;
st755:
	if ( ++( p) == ( pe) )
		goto _test_eof755;
case 755:
	switch( (*( p)) ) {
		case 72: goto st756;
		case 104: goto st756;
	}
	goto tr257;
st756:
	if ( ++( p) == ( pe) )
		goto _test_eof756;
case 756:
	switch( (*( p)) ) {
		case 65: goto st757;
		case 97: goto st757;
	}
	goto tr257;
st757:
	if ( ++( p) == ( pe) )
		goto _test_eof757;
case 757:
	switch( (*( p)) ) {
		case 78: goto st758;
		case 110: goto st758;
	}
	goto tr257;
st758:
	if ( ++( p) == ( pe) )
		goto _test_eof758;
case 758:
	switch( (*( p)) ) {
		case 71: goto st759;
		case 103: goto st759;
	}
	goto tr257;
st759:
	if ( ++( p) == ( pe) )
		goto _test_eof759;
case 759:
	switch( (*( p)) ) {
		case 69: goto st760;
		case 101: goto st760;
	}
	goto tr257;
st760:
	if ( ++( p) == ( pe) )
		goto _test_eof760;
case 760:
	switch( (*( p)) ) {
		case 83: goto st761;
		case 115: goto st761;
	}
	goto tr257;
st761:
	if ( ++( p) == ( pe) )
		goto _test_eof761;
case 761:
	if ( (*( p)) == 32 )
		goto st762;
	goto tr257;
st762:
	if ( ++( p) == ( pe) )
		goto _test_eof762;
case 762:
	if ( (*( p)) == 35 )
		goto st763;
	goto tr257;
st763:
	if ( ++( p) == ( pe) )
		goto _test_eof763;
case 763:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr940;
	goto tr257;
tr940:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1855;
st1855:
	if ( ++( p) == ( pe) )
		goto _test_eof1855;
case 1855:
#line 17737 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1855;
	goto tr2372;
tr2361:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1856;
st1856:
	if ( ++( p) == ( pe) )
		goto _test_eof1856;
case 1856:
#line 17751 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2374;
		case 91: goto tr2207;
		case 108: goto tr2374;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2374:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1857;
st1857:
	if ( ++( p) == ( pe) )
		goto _test_eof1857;
case 1857:
#line 17777 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2375;
		case 91: goto tr2207;
		case 108: goto tr2375;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2375:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1858;
st1858:
	if ( ++( p) == ( pe) )
		goto _test_eof1858;
case 1858:
#line 17803 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st764;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st764:
	if ( ++( p) == ( pe) )
		goto _test_eof764;
case 764:
	if ( (*( p)) == 35 )
		goto st765;
	goto tr257;
st765:
	if ( ++( p) == ( pe) )
		goto _test_eof765;
case 765:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr942;
	goto tr257;
tr942:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1859;
st1859:
	if ( ++( p) == ( pe) )
		goto _test_eof1859;
case 1859:
#line 17840 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1859;
	goto tr2377;
tr2167:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1860;
st1860:
	if ( ++( p) == ( pe) )
		goto _test_eof1860;
case 1860:
#line 17856 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2379;
		case 91: goto tr2207;
		case 101: goto tr2379;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2379:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1861;
st1861:
	if ( ++( p) == ( pe) )
		goto _test_eof1861;
case 1861:
#line 17882 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2380;
		case 91: goto tr2207;
		case 99: goto tr2380;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2380:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1862;
st1862:
	if ( ++( p) == ( pe) )
		goto _test_eof1862;
case 1862:
#line 17908 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2381;
		case 91: goto tr2207;
		case 111: goto tr2381;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2381:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1863;
st1863:
	if ( ++( p) == ( pe) )
		goto _test_eof1863;
case 1863:
#line 17934 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2382;
		case 91: goto tr2207;
		case 114: goto tr2382;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2382:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1864;
st1864:
	if ( ++( p) == ( pe) )
		goto _test_eof1864;
case 1864:
#line 17960 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2383;
		case 91: goto tr2207;
		case 100: goto tr2383;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2383:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1865;
st1865:
	if ( ++( p) == ( pe) )
		goto _test_eof1865;
case 1865:
#line 17986 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st766;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st766:
	if ( ++( p) == ( pe) )
		goto _test_eof766;
case 766:
	if ( (*( p)) == 35 )
		goto st767;
	goto tr257;
st767:
	if ( ++( p) == ( pe) )
		goto _test_eof767;
case 767:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr944;
	goto tr257;
tr944:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1866;
st1866:
	if ( ++( p) == ( pe) )
		goto _test_eof1866;
case 1866:
#line 18023 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1866;
	goto tr2385;
tr2168:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1867;
st1867:
	if ( ++( p) == ( pe) )
		goto _test_eof1867;
case 1867:
#line 18039 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2387;
		case 91: goto tr2207;
		case 101: goto tr2387;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2387:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1868;
st1868:
	if ( ++( p) == ( pe) )
		goto _test_eof1868;
case 1868:
#line 18065 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2388;
		case 91: goto tr2207;
		case 116: goto tr2388;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2388:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1869;
st1869:
	if ( ++( p) == ( pe) )
		goto _test_eof1869;
case 1869:
#line 18091 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st768;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st768:
	if ( ++( p) == ( pe) )
		goto _test_eof768;
case 768:
	if ( (*( p)) == 35 )
		goto st769;
	goto tr257;
st769:
	if ( ++( p) == ( pe) )
		goto _test_eof769;
case 769:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr946;
	goto tr257;
tr946:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1870;
st1870:
	if ( ++( p) == ( pe) )
		goto _test_eof1870;
case 1870:
#line 18128 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1870;
	goto tr2390;
tr2169:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1871;
st1871:
	if ( ++( p) == ( pe) )
		goto _test_eof1871;
case 1871:
#line 18144 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2392;
		case 72: goto tr2393;
		case 73: goto tr2394;
		case 79: goto tr2395;
		case 91: goto tr2207;
		case 97: goto tr2392;
		case 104: goto tr2393;
		case 105: goto tr2394;
		case 111: goto tr2395;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2392:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1872;
st1872:
	if ( ++( p) == ( pe) )
		goto _test_eof1872;
case 1872:
#line 18176 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2396;
		case 91: goto tr2207;
		case 107: goto tr2396;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2396:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1873;
st1873:
	if ( ++( p) == ( pe) )
		goto _test_eof1873;
case 1873:
#line 18202 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2397;
		case 91: goto tr2207;
		case 101: goto tr2397;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2397:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1874;
st1874:
	if ( ++( p) == ( pe) )
		goto _test_eof1874;
case 1874:
#line 18228 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st770;
		case 68: goto tr2399;
		case 91: goto tr2207;
		case 100: goto tr2399;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st770:
	if ( ++( p) == ( pe) )
		goto _test_eof770;
case 770:
	switch( (*( p)) ) {
		case 68: goto st771;
		case 100: goto st771;
	}
	goto tr257;
st771:
	if ( ++( p) == ( pe) )
		goto _test_eof771;
case 771:
	switch( (*( p)) ) {
		case 79: goto st772;
		case 111: goto st772;
	}
	goto tr257;
st772:
	if ( ++( p) == ( pe) )
		goto _test_eof772;
case 772:
	switch( (*( p)) ) {
		case 87: goto st773;
		case 119: goto st773;
	}
	goto tr257;
st773:
	if ( ++( p) == ( pe) )
		goto _test_eof773;
case 773:
	switch( (*( p)) ) {
		case 78: goto st774;
		case 110: goto st774;
	}
	goto tr257;
st774:
	if ( ++( p) == ( pe) )
		goto _test_eof774;
case 774:
	if ( (*( p)) == 32 )
		goto st775;
	goto tr257;
st775:
	if ( ++( p) == ( pe) )
		goto _test_eof775;
case 775:
	switch( (*( p)) ) {
		case 35: goto st776;
		case 82: goto st777;
		case 114: goto st777;
	}
	goto tr257;
st776:
	if ( ++( p) == ( pe) )
		goto _test_eof776;
case 776:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr954;
	goto tr257;
tr954:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1875;
st1875:
	if ( ++( p) == ( pe) )
		goto _test_eof1875;
case 1875:
#line 18313 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1875;
	goto tr2400;
st777:
	if ( ++( p) == ( pe) )
		goto _test_eof777;
case 777:
	switch( (*( p)) ) {
		case 69: goto st778;
		case 101: goto st778;
	}
	goto tr257;
st778:
	if ( ++( p) == ( pe) )
		goto _test_eof778;
case 778:
	switch( (*( p)) ) {
		case 81: goto st779;
		case 113: goto st779;
	}
	goto tr257;
st779:
	if ( ++( p) == ( pe) )
		goto _test_eof779;
case 779:
	switch( (*( p)) ) {
		case 85: goto st780;
		case 117: goto st780;
	}
	goto tr257;
st780:
	if ( ++( p) == ( pe) )
		goto _test_eof780;
case 780:
	switch( (*( p)) ) {
		case 69: goto st781;
		case 101: goto st781;
	}
	goto tr257;
st781:
	if ( ++( p) == ( pe) )
		goto _test_eof781;
case 781:
	switch( (*( p)) ) {
		case 83: goto st782;
		case 115: goto st782;
	}
	goto tr257;
st782:
	if ( ++( p) == ( pe) )
		goto _test_eof782;
case 782:
	switch( (*( p)) ) {
		case 84: goto st783;
		case 116: goto st783;
	}
	goto tr257;
st783:
	if ( ++( p) == ( pe) )
		goto _test_eof783;
case 783:
	if ( (*( p)) == 32 )
		goto st784;
	goto tr257;
st784:
	if ( ++( p) == ( pe) )
		goto _test_eof784;
case 784:
	if ( (*( p)) == 35 )
		goto st776;
	goto tr257;
tr2399:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1876;
st1876:
	if ( ++( p) == ( pe) )
		goto _test_eof1876;
case 1876:
#line 18395 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2402;
		case 91: goto tr2207;
		case 111: goto tr2402;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2402:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1877;
st1877:
	if ( ++( p) == ( pe) )
		goto _test_eof1877;
case 1877:
#line 18421 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 87: goto tr2403;
		case 91: goto tr2207;
		case 119: goto tr2403;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2403:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1878;
st1878:
	if ( ++( p) == ( pe) )
		goto _test_eof1878;
case 1878:
#line 18447 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2404;
		case 91: goto tr2207;
		case 110: goto tr2404;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2404:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1879;
st1879:
	if ( ++( p) == ( pe) )
		goto _test_eof1879;
case 1879:
#line 18473 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st775;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2393:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1880;
st1880:
	if ( ++( p) == ( pe) )
		goto _test_eof1880;
case 1880:
#line 18498 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2405;
		case 91: goto tr2207;
		case 117: goto tr2405;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2405:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1881;
st1881:
	if ( ++( p) == ( pe) )
		goto _test_eof1881;
case 1881:
#line 18524 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2406;
		case 91: goto tr2207;
		case 109: goto tr2406;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2406:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1882;
st1882:
	if ( ++( p) == ( pe) )
		goto _test_eof1882;
case 1882:
#line 18550 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 66: goto tr2407;
		case 91: goto tr2207;
		case 98: goto tr2407;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2407:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1883;
st1883:
	if ( ++( p) == ( pe) )
		goto _test_eof1883;
case 1883:
#line 18576 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st785;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st785:
	if ( ++( p) == ( pe) )
		goto _test_eof785;
case 785:
	if ( (*( p)) == 35 )
		goto st786;
	goto tr257;
st786:
	if ( ++( p) == ( pe) )
		goto _test_eof786;
case 786:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr963;
	goto tr257;
tr963:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1884;
st1884:
	if ( ++( p) == ( pe) )
		goto _test_eof1884;
case 1884:
#line 18613 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1884;
	goto tr2409;
tr2394:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1885;
st1885:
	if ( ++( p) == ( pe) )
		goto _test_eof1885;
case 1885:
#line 18627 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2411;
		case 91: goto tr2207;
		case 99: goto tr2411;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2411:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1886;
st1886:
	if ( ++( p) == ( pe) )
		goto _test_eof1886;
case 1886:
#line 18653 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2412;
		case 91: goto tr2207;
		case 107: goto tr2412;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2412:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1887;
st1887:
	if ( ++( p) == ( pe) )
		goto _test_eof1887;
case 1887:
#line 18679 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2413;
		case 91: goto tr2207;
		case 101: goto tr2413;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2413:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1888;
st1888:
	if ( ++( p) == ( pe) )
		goto _test_eof1888;
case 1888:
#line 18705 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2414;
		case 91: goto tr2207;
		case 116: goto tr2414;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2414:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1889;
st1889:
	if ( ++( p) == ( pe) )
		goto _test_eof1889;
case 1889:
#line 18731 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st787;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st787:
	if ( ++( p) == ( pe) )
		goto _test_eof787;
case 787:
	if ( (*( p)) == 35 )
		goto st788;
	goto tr257;
st788:
	if ( ++( p) == ( pe) )
		goto _test_eof788;
case 788:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr965;
	goto tr257;
tr965:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1890;
st1890:
	if ( ++( p) == ( pe) )
		goto _test_eof1890;
case 1890:
#line 18768 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1890;
	goto tr2416;
tr2395:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1891;
st1891:
	if ( ++( p) == ( pe) )
		goto _test_eof1891;
case 1891:
#line 18782 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2418;
		case 91: goto tr2207;
		case 112: goto tr2418;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2418:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1892;
st1892:
	if ( ++( p) == ( pe) )
		goto _test_eof1892;
case 1892:
#line 18808 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2419;
		case 91: goto tr2207;
		case 105: goto tr2419;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2419:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1893;
st1893:
	if ( ++( p) == ( pe) )
		goto _test_eof1893;
case 1893:
#line 18834 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2420;
		case 91: goto tr2207;
		case 99: goto tr2420;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2420:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1894;
st1894:
	if ( ++( p) == ( pe) )
		goto _test_eof1894;
case 1894:
#line 18860 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st679;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2170:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1895;
st1895:
	if ( ++( p) == ( pe) )
		goto _test_eof1895;
case 1895:
#line 18887 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2421;
		case 91: goto tr2207;
		case 115: goto tr2421;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2421:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1896;
st1896:
	if ( ++( p) == ( pe) )
		goto _test_eof1896;
case 1896:
#line 18913 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2422;
		case 91: goto tr2207;
		case 101: goto tr2422;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2422:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1897;
st1897:
	if ( ++( p) == ( pe) )
		goto _test_eof1897;
case 1897:
#line 18939 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2423;
		case 91: goto tr2207;
		case 114: goto tr2423;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2423:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1898;
st1898:
	if ( ++( p) == ( pe) )
		goto _test_eof1898;
case 1898:
#line 18965 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st789;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st789:
	if ( ++( p) == ( pe) )
		goto _test_eof789;
case 789:
	if ( (*( p)) == 35 )
		goto st790;
	goto tr257;
st790:
	if ( ++( p) == ( pe) )
		goto _test_eof790;
case 790:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr967;
	goto tr257;
tr967:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1899;
st1899:
	if ( ++( p) == ( pe) )
		goto _test_eof1899;
case 1899:
#line 19002 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1899;
	goto tr2425;
tr2171:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1900;
st1900:
	if ( ++( p) == ( pe) )
		goto _test_eof1900;
case 1900:
#line 19018 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2427;
		case 91: goto tr2207;
		case 105: goto tr2427;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2427:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1901;
st1901:
	if ( ++( p) == ( pe) )
		goto _test_eof1901;
case 1901:
#line 19044 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2428;
		case 91: goto tr2207;
		case 107: goto tr2428;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2428:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1902;
st1902:
	if ( ++( p) == ( pe) )
		goto _test_eof1902;
case 1902:
#line 19070 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2429;
		case 91: goto tr2207;
		case 105: goto tr2429;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2429:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1903;
st1903:
	if ( ++( p) == ( pe) )
		goto _test_eof1903;
case 1903:
#line 19096 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st791;
		case 67: goto tr2431;
		case 80: goto tr2432;
		case 91: goto tr2207;
		case 99: goto tr2431;
		case 112: goto tr2432;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
st791:
	if ( ++( p) == ( pe) )
		goto _test_eof791;
case 791:
	switch( (*( p)) ) {
		case 32: goto st792;
		case 35: goto st793;
		case 67: goto st794;
		case 80: goto st803;
		case 99: goto st794;
		case 112: goto st803;
	}
	goto tr257;
st792:
	if ( ++( p) == ( pe) )
		goto _test_eof792;
case 792:
	if ( (*( p)) == 35 )
		goto st793;
	goto tr257;
st793:
	if ( ++( p) == ( pe) )
		goto _test_eof793;
case 793:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr972;
	goto tr257;
tr972:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1904;
st1904:
	if ( ++( p) == ( pe) )
		goto _test_eof1904;
case 1904:
#line 19150 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1904;
	goto tr2433;
st794:
	if ( ++( p) == ( pe) )
		goto _test_eof794;
case 794:
	switch( (*( p)) ) {
		case 72: goto st795;
		case 104: goto st795;
	}
	goto tr257;
st795:
	if ( ++( p) == ( pe) )
		goto _test_eof795;
case 795:
	switch( (*( p)) ) {
		case 65: goto st796;
		case 97: goto st796;
	}
	goto tr257;
st796:
	if ( ++( p) == ( pe) )
		goto _test_eof796;
case 796:
	switch( (*( p)) ) {
		case 78: goto st797;
		case 110: goto st797;
	}
	goto tr257;
st797:
	if ( ++( p) == ( pe) )
		goto _test_eof797;
case 797:
	switch( (*( p)) ) {
		case 71: goto st798;
		case 103: goto st798;
	}
	goto tr257;
st798:
	if ( ++( p) == ( pe) )
		goto _test_eof798;
case 798:
	switch( (*( p)) ) {
		case 69: goto st799;
		case 101: goto st799;
	}
	goto tr257;
st799:
	if ( ++( p) == ( pe) )
		goto _test_eof799;
case 799:
	switch( (*( p)) ) {
		case 83: goto st800;
		case 115: goto st800;
	}
	goto tr257;
st800:
	if ( ++( p) == ( pe) )
		goto _test_eof800;
case 800:
	if ( (*( p)) == 32 )
		goto st801;
	goto tr257;
st801:
	if ( ++( p) == ( pe) )
		goto _test_eof801;
case 801:
	if ( (*( p)) == 35 )
		goto st802;
	goto tr257;
st802:
	if ( ++( p) == ( pe) )
		goto _test_eof802;
case 802:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr981;
	goto tr257;
tr981:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1905;
st1905:
	if ( ++( p) == ( pe) )
		goto _test_eof1905;
case 1905:
#line 19237 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1905;
	goto tr2435;
st803:
	if ( ++( p) == ( pe) )
		goto _test_eof803;
case 803:
	switch( (*( p)) ) {
		case 65: goto st804;
		case 97: goto st804;
	}
	goto tr257;
st804:
	if ( ++( p) == ( pe) )
		goto _test_eof804;
case 804:
	switch( (*( p)) ) {
		case 71: goto st805;
		case 103: goto st805;
	}
	goto tr257;
st805:
	if ( ++( p) == ( pe) )
		goto _test_eof805;
case 805:
	switch( (*( p)) ) {
		case 69: goto st806;
		case 101: goto st806;
	}
	goto tr257;
st806:
	if ( ++( p) == ( pe) )
		goto _test_eof806;
case 806:
	if ( (*( p)) == 32 )
		goto st807;
	goto tr257;
st807:
	if ( ++( p) == ( pe) )
		goto _test_eof807;
case 807:
	switch( (*( p)) ) {
		case 32: goto st801;
		case 35: goto st808;
		case 67: goto st794;
		case 99: goto st794;
	}
	goto tr257;
st808:
	if ( ++( p) == ( pe) )
		goto _test_eof808;
case 808:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr987;
	goto tr257;
tr987:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1906;
st1906:
	if ( ++( p) == ( pe) )
		goto _test_eof1906;
case 1906:
#line 19301 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1906;
	goto tr2433;
tr2431:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1907;
st1907:
	if ( ++( p) == ( pe) )
		goto _test_eof1907;
case 1907:
#line 19315 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 72: goto tr2438;
		case 91: goto tr2207;
		case 104: goto tr2438;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2438:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1908;
st1908:
	if ( ++( p) == ( pe) )
		goto _test_eof1908;
case 1908:
#line 19341 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2439;
		case 91: goto tr2207;
		case 97: goto tr2439;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2439:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1909;
st1909:
	if ( ++( p) == ( pe) )
		goto _test_eof1909;
case 1909:
#line 19367 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2440;
		case 91: goto tr2207;
		case 110: goto tr2440;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2440:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1910;
st1910:
	if ( ++( p) == ( pe) )
		goto _test_eof1910;
case 1910:
#line 19393 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2441;
		case 91: goto tr2207;
		case 103: goto tr2441;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2441:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1911;
st1911:
	if ( ++( p) == ( pe) )
		goto _test_eof1911;
case 1911:
#line 19419 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2442;
		case 91: goto tr2207;
		case 101: goto tr2442;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2442:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1912;
st1912:
	if ( ++( p) == ( pe) )
		goto _test_eof1912;
case 1912:
#line 19445 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2443;
		case 91: goto tr2207;
		case 115: goto tr2443;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2443:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1913;
st1913:
	if ( ++( p) == ( pe) )
		goto _test_eof1913;
case 1913:
#line 19471 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st801;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2432:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1914;
st1914:
	if ( ++( p) == ( pe) )
		goto _test_eof1914;
case 1914:
#line 19496 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2444;
		case 91: goto tr2207;
		case 97: goto tr2444;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2444:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1915;
st1915:
	if ( ++( p) == ( pe) )
		goto _test_eof1915;
case 1915:
#line 19522 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2445;
		case 91: goto tr2207;
		case 103: goto tr2445;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2445:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1916;
st1916:
	if ( ++( p) == ( pe) )
		goto _test_eof1916;
case 1916:
#line 19548 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2446;
		case 91: goto tr2207;
		case 101: goto tr2446;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2446:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1917;
st1917:
	if ( ++( p) == ( pe) )
		goto _test_eof1917;
case 1917:
#line 19574 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st807;
		case 91: goto tr2207;
		case 123: goto tr2208;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2178;
tr2172:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1918;
st1918:
	if ( ++( p) == ( pe) )
		goto _test_eof1918;
case 1918:
#line 19603 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2177;
		case 10: goto tr2177;
		case 13: goto tr2177;
		case 35: goto tr2448;
		case 47: goto tr2449;
		case 66: goto tr2450;
		case 67: goto tr2451;
		case 72: goto tr2452;
		case 73: goto tr2453;
		case 78: goto tr2454;
		case 81: goto tr2455;
		case 83: goto tr2456;
		case 84: goto tr2457;
		case 85: goto tr2458;
		case 91: goto tr2459;
		case 98: goto tr2450;
		case 99: goto tr2451;
		case 104: goto tr2452;
		case 105: goto tr2453;
		case 110: goto tr2454;
		case 113: goto tr2455;
		case 115: goto tr2456;
		case 116: goto tr2457;
		case 117: goto tr2458;
	}
	goto tr2447;
tr2447:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st809;
st809:
	if ( ++( p) == ( pe) )
		goto _test_eof809;
case 809:
#line 19639 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
	}
	goto st809;
tr989:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st810;
st810:
	if ( ++( p) == ( pe) )
		goto _test_eof810;
case 810:
#line 19655 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st811;
	goto tr261;
st811:
	if ( ++( p) == ( pe) )
		goto _test_eof811;
case 811:
	switch( (*( p)) ) {
		case 35: goto tr991;
		case 47: goto tr991;
		case 72: goto tr992;
		case 104: goto tr992;
	}
	goto tr254;
tr991:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st812;
st812:
	if ( ++( p) == ( pe) )
		goto _test_eof812;
case 812:
#line 19678 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 32: goto tr254;
		case 41: goto tr994;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st812;
tr992:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st813;
st813:
	if ( ++( p) == ( pe) )
		goto _test_eof813;
case 813:
#line 19695 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st814;
		case 116: goto st814;
	}
	goto tr254;
st814:
	if ( ++( p) == ( pe) )
		goto _test_eof814;
case 814:
	switch( (*( p)) ) {
		case 84: goto st815;
		case 116: goto st815;
	}
	goto tr254;
st815:
	if ( ++( p) == ( pe) )
		goto _test_eof815;
case 815:
	switch( (*( p)) ) {
		case 80: goto st816;
		case 112: goto st816;
	}
	goto tr254;
st816:
	if ( ++( p) == ( pe) )
		goto _test_eof816;
case 816:
	switch( (*( p)) ) {
		case 58: goto st817;
		case 83: goto st820;
		case 115: goto st820;
	}
	goto tr254;
st817:
	if ( ++( p) == ( pe) )
		goto _test_eof817;
case 817:
	if ( (*( p)) == 47 )
		goto st818;
	goto tr254;
st818:
	if ( ++( p) == ( pe) )
		goto _test_eof818;
case 818:
	if ( (*( p)) == 47 )
		goto st819;
	goto tr254;
st819:
	if ( ++( p) == ( pe) )
		goto _test_eof819;
case 819:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 32: goto tr254;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st812;
st820:
	if ( ++( p) == ( pe) )
		goto _test_eof820;
case 820:
	if ( (*( p)) == 58 )
		goto st817;
	goto tr254;
tr2448:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st821;
st821:
	if ( ++( p) == ( pe) )
		goto _test_eof821;
case 821:
#line 19771 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 45: goto tr1003;
		case 93: goto tr1004;
		case 95: goto tr1003;
	}
	if ( (*( p)) < 48 ) {
		if ( 9 <= (*( p)) && (*( p)) <= 12 )
			goto st809;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr1003;
		} else if ( (*( p)) >= 65 )
			goto tr1003;
	} else
		goto tr1003;
	goto st822;
st822:
	if ( ++( p) == ( pe) )
		goto _test_eof822;
case 822:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1004;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
tr1004:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st823;
st823:
	if ( ++( p) == ( pe) )
		goto _test_eof823;
case 823:
#line 19817 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st824;
	goto tr261;
st824:
	if ( ++( p) == ( pe) )
		goto _test_eof824;
case 824:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 35: goto tr1007;
		case 47: goto tr1007;
		case 72: goto tr1008;
		case 104: goto tr1008;
	}
	goto tr1006;
tr1006:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st825;
st825:
	if ( ++( p) == ( pe) )
		goto _test_eof825;
case 825:
#line 19843 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 41: goto tr1010;
	}
	goto st825;
tr1283:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st826;
tr1007:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st826;
st826:
	if ( ++( p) == ( pe) )
		goto _test_eof826;
case 826:
#line 19865 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto st825;
		case 41: goto tr1012;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st825;
	goto st826;
tr1008:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st827;
st827:
	if ( ++( p) == ( pe) )
		goto _test_eof827;
case 827:
#line 19886 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 41: goto tr1010;
		case 84: goto st828;
		case 116: goto st828;
	}
	goto st825;
st828:
	if ( ++( p) == ( pe) )
		goto _test_eof828;
case 828:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 41: goto tr1010;
		case 84: goto st829;
		case 116: goto st829;
	}
	goto st825;
st829:
	if ( ++( p) == ( pe) )
		goto _test_eof829;
case 829:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 41: goto tr1010;
		case 80: goto st830;
		case 112: goto st830;
	}
	goto st825;
st830:
	if ( ++( p) == ( pe) )
		goto _test_eof830;
case 830:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 41: goto tr1010;
		case 58: goto st831;
		case 83: goto st834;
		case 115: goto st834;
	}
	goto st825;
st831:
	if ( ++( p) == ( pe) )
		goto _test_eof831;
case 831:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 41: goto tr1010;
		case 47: goto st832;
	}
	goto st825;
st832:
	if ( ++( p) == ( pe) )
		goto _test_eof832;
case 832:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 41: goto tr1010;
		case 47: goto st833;
	}
	goto st825;
st833:
	if ( ++( p) == ( pe) )
		goto _test_eof833;
case 833:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto st825;
		case 41: goto tr1020;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st825;
	goto st826;
tr1020:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 402 "ext/dtext/dtext.cpp.rl"
	{( act) = 54;}
	goto st1919;
tr1376:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 398 "ext/dtext/dtext.cpp.rl"
	{( act) = 53;}
	goto st1919;
st1919:
	if ( ++( p) == ( pe) )
		goto _test_eof1919;
case 1919:
#line 19992 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 32: goto tr254;
		case 41: goto tr994;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st812;
st834:
	if ( ++( p) == ( pe) )
		goto _test_eof834;
case 834:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
		case 41: goto tr1010;
		case 58: goto st831;
	}
	goto st825;
tr1003:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st835;
st835:
	if ( ++( p) == ( pe) )
		goto _test_eof835;
case 835:
#line 20021 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 45: goto st835;
		case 93: goto tr1022;
		case 95: goto st835;
	}
	if ( (*( p)) < 48 ) {
		if ( 9 <= (*( p)) && (*( p)) <= 12 )
			goto st809;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st835;
		} else if ( (*( p)) >= 65 )
			goto st835;
	} else
		goto st835;
	goto st822;
tr1022:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 490 "ext/dtext/dtext.cpp.rl"
	{( act) = 78;}
	goto st1920;
tr1043:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 484 "ext/dtext/dtext.cpp.rl"
	{( act) = 77;}
	goto st1920;
tr1071:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 439 "ext/dtext/dtext.cpp.rl"
	{( act) = 71;}
	goto st1920;
tr1072:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 437 "ext/dtext/dtext.cpp.rl"
	{( act) = 69;}
	goto st1920;
st1920:
	if ( ++( p) == ( pe) )
		goto _test_eof1920;
case 1920:
#line 20089 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st824;
	goto tr254;
tr2449:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st836;
st836:
	if ( ++( p) == ( pe) )
		goto _test_eof836;
case 836:
#line 20103 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 66: goto st837;
		case 67: goto st839;
		case 72: goto st849;
		case 73: goto st850;
		case 78: goto st851;
		case 81: goto st857;
		case 83: goto st862;
		case 84: goto st879;
		case 85: goto st890;
		case 93: goto tr1004;
		case 98: goto st837;
		case 99: goto st839;
		case 104: goto st849;
		case 105: goto st850;
		case 110: goto st851;
		case 113: goto st857;
		case 115: goto st862;
		case 116: goto st879;
		case 117: goto st890;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st837:
	if ( ++( p) == ( pe) )
		goto _test_eof837;
case 837:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 82: goto st838;
		case 93: goto tr1033;
		case 114: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st838:
	if ( ++( p) == ( pe) )
		goto _test_eof838;
case 838:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr261;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st839:
	if ( ++( p) == ( pe) )
		goto _test_eof839;
case 839:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 79: goto st840;
		case 93: goto tr1004;
		case 111: goto st840;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st840:
	if ( ++( p) == ( pe) )
		goto _test_eof840;
case 840:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 68: goto st841;
		case 76: goto st842;
		case 93: goto tr1004;
		case 100: goto st841;
		case 108: goto st842;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st841:
	if ( ++( p) == ( pe) )
		goto _test_eof841;
case 841:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 69: goto st838;
		case 93: goto tr1004;
		case 101: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st842:
	if ( ++( p) == ( pe) )
		goto _test_eof842;
case 842:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 71: goto st843;
		case 79: goto st847;
		case 93: goto tr261;
		case 103: goto st843;
		case 111: goto st847;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st843:
	if ( ++( p) == ( pe) )
		goto _test_eof843;
case 843:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 82: goto st844;
		case 93: goto tr1004;
		case 114: goto st844;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st844:
	if ( ++( p) == ( pe) )
		goto _test_eof844;
case 844:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 79: goto st845;
		case 93: goto tr1004;
		case 111: goto st845;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st845:
	if ( ++( p) == ( pe) )
		goto _test_eof845;
case 845:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 85: goto st846;
		case 93: goto tr1004;
		case 117: goto st846;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st846:
	if ( ++( p) == ( pe) )
		goto _test_eof846;
case 846:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 80: goto st838;
		case 93: goto tr1004;
		case 112: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st847:
	if ( ++( p) == ( pe) )
		goto _test_eof847;
case 847:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 82: goto st848;
		case 93: goto tr1004;
		case 114: goto st848;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st848:
	if ( ++( p) == ( pe) )
		goto _test_eof848;
case 848:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1043;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st849:
	if ( ++( p) == ( pe) )
		goto _test_eof849;
case 849:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 82: goto st838;
		case 93: goto tr1004;
		case 114: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st850:
	if ( ++( p) == ( pe) )
		goto _test_eof850;
case 850:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1044;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st851:
	if ( ++( p) == ( pe) )
		goto _test_eof851;
case 851:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 79: goto st852;
		case 93: goto tr1004;
		case 111: goto st852;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st852:
	if ( ++( p) == ( pe) )
		goto _test_eof852;
case 852:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 68: goto st853;
		case 93: goto tr1004;
		case 100: goto st853;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st853:
	if ( ++( p) == ( pe) )
		goto _test_eof853;
case 853:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 84: goto st854;
		case 93: goto tr1004;
		case 116: goto st854;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st854:
	if ( ++( p) == ( pe) )
		goto _test_eof854;
case 854:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 69: goto st855;
		case 93: goto tr1004;
		case 101: goto st855;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st855:
	if ( ++( p) == ( pe) )
		goto _test_eof855;
case 855:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 88: goto st856;
		case 93: goto tr1004;
		case 120: goto st856;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st856:
	if ( ++( p) == ( pe) )
		goto _test_eof856;
case 856:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 84: goto st838;
		case 93: goto tr1004;
		case 116: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st857:
	if ( ++( p) == ( pe) )
		goto _test_eof857;
case 857:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 85: goto st858;
		case 93: goto tr1004;
		case 117: goto st858;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st858:
	if ( ++( p) == ( pe) )
		goto _test_eof858;
case 858:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 79: goto st859;
		case 93: goto tr1004;
		case 111: goto st859;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st859:
	if ( ++( p) == ( pe) )
		goto _test_eof859;
case 859:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 84: goto st860;
		case 93: goto tr1004;
		case 116: goto st860;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st860:
	if ( ++( p) == ( pe) )
		goto _test_eof860;
case 860:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 69: goto st861;
		case 93: goto tr1004;
		case 101: goto st861;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st861:
	if ( ++( p) == ( pe) )
		goto _test_eof861;
case 861:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	switch( _widec ) {
		case 9: goto st809;
		case 32: goto st809;
		case 2653: goto st1716;
	}
	if ( _widec < 11 ) {
		if ( _widec > -1 ) {
			if ( 1 <= _widec && _widec <= 8 )
				goto st822;
		} else
			goto st822;
	} else if ( _widec > 12 ) {
		if ( _widec > 92 ) {
			if ( 94 <= _widec )
				goto st822;
		} else if ( _widec >= 14 )
			goto st822;
	} else
		goto st809;
	goto tr261;
st862:
	if ( ++( p) == ( pe) )
		goto _test_eof862;
case 862:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 69: goto st863;
		case 80: goto st869;
		case 85: goto st876;
		case 93: goto tr1057;
		case 101: goto st863;
		case 112: goto st869;
		case 117: goto st876;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st863:
	if ( ++( p) == ( pe) )
		goto _test_eof863;
case 863:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 67: goto st864;
		case 93: goto tr1004;
		case 99: goto st864;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st864:
	if ( ++( p) == ( pe) )
		goto _test_eof864;
case 864:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 84: goto st865;
		case 93: goto tr1004;
		case 116: goto st865;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st865:
	if ( ++( p) == ( pe) )
		goto _test_eof865;
case 865:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 73: goto st866;
		case 93: goto tr1004;
		case 105: goto st866;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st866:
	if ( ++( p) == ( pe) )
		goto _test_eof866;
case 866:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 79: goto st867;
		case 93: goto tr1004;
		case 111: goto st867;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st867:
	if ( ++( p) == ( pe) )
		goto _test_eof867;
case 867:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 78: goto st868;
		case 93: goto tr1004;
		case 110: goto st868;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st868:
	if ( ++( p) == ( pe) )
		goto _test_eof868;
case 868:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_SECTION)  ) _widec += 256;
	}
	switch( _widec ) {
		case 9: goto st809;
		case 32: goto st809;
		case 3165: goto st1717;
	}
	if ( _widec < 11 ) {
		if ( _widec > -1 ) {
			if ( 1 <= _widec && _widec <= 8 )
				goto st822;
		} else
			goto st822;
	} else if ( _widec > 12 ) {
		if ( _widec > 92 ) {
			if ( 94 <= _widec )
				goto st822;
		} else if ( _widec >= 14 )
			goto st822;
	} else
		goto st809;
	goto tr261;
st869:
	if ( ++( p) == ( pe) )
		goto _test_eof869;
case 869:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 79: goto st870;
		case 93: goto tr1004;
		case 111: goto st870;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st870:
	if ( ++( p) == ( pe) )
		goto _test_eof870;
case 870:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 73: goto st871;
		case 93: goto tr1004;
		case 105: goto st871;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st871:
	if ( ++( p) == ( pe) )
		goto _test_eof871;
case 871:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 76: goto st872;
		case 93: goto tr1004;
		case 108: goto st872;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st872:
	if ( ++( p) == ( pe) )
		goto _test_eof872;
case 872:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 69: goto st873;
		case 93: goto tr1004;
		case 101: goto st873;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st873:
	if ( ++( p) == ( pe) )
		goto _test_eof873;
case 873:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 82: goto st874;
		case 93: goto tr1004;
		case 114: goto st874;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st874:
	if ( ++( p) == ( pe) )
		goto _test_eof874;
case 874:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 83: goto st875;
		case 93: goto tr373;
		case 115: goto st875;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st875:
	if ( ++( p) == ( pe) )
		goto _test_eof875;
case 875:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr373;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st876:
	if ( ++( p) == ( pe) )
		goto _test_eof876;
case 876:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 66: goto st877;
		case 80: goto st878;
		case 93: goto tr1004;
		case 98: goto st877;
		case 112: goto st878;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st877:
	if ( ++( p) == ( pe) )
		goto _test_eof877;
case 877:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1071;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st878:
	if ( ++( p) == ( pe) )
		goto _test_eof878;
case 878:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1072;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st879:
	if ( ++( p) == ( pe) )
		goto _test_eof879;
case 879:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 65: goto st880;
		case 66: goto st882;
		case 68: goto st885;
		case 72: goto st886;
		case 78: goto st889;
		case 82: goto st838;
		case 93: goto tr1004;
		case 97: goto st880;
		case 98: goto st882;
		case 100: goto st885;
		case 104: goto st886;
		case 110: goto st889;
		case 114: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st880:
	if ( ++( p) == ( pe) )
		goto _test_eof880;
case 880:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 66: goto st881;
		case 93: goto tr1004;
		case 98: goto st881;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st881:
	if ( ++( p) == ( pe) )
		goto _test_eof881;
case 881:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 76: goto st841;
		case 93: goto tr1004;
		case 108: goto st841;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st882:
	if ( ++( p) == ( pe) )
		goto _test_eof882;
case 882:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 79: goto st883;
		case 93: goto tr1004;
		case 111: goto st883;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st883:
	if ( ++( p) == ( pe) )
		goto _test_eof883;
case 883:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 68: goto st884;
		case 93: goto tr1004;
		case 100: goto st884;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st884:
	if ( ++( p) == ( pe) )
		goto _test_eof884;
case 884:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 89: goto st838;
		case 93: goto tr1004;
		case 121: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st885:
	if ( ++( p) == ( pe) )
		goto _test_eof885;
case 885:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr299;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st886:
	if ( ++( p) == ( pe) )
		goto _test_eof886;
case 886:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 69: goto st887;
		case 93: goto tr300;
		case 101: goto st887;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st887:
	if ( ++( p) == ( pe) )
		goto _test_eof887;
case 887:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 65: goto st888;
		case 93: goto tr1004;
		case 97: goto st888;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st888:
	if ( ++( p) == ( pe) )
		goto _test_eof888;
case 888:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 68: goto st838;
		case 93: goto tr1004;
		case 100: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st889:
	if ( ++( p) == ( pe) )
		goto _test_eof889;
case 889:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr352;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st890:
	if ( ++( p) == ( pe) )
		goto _test_eof890;
case 890:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 82: goto st891;
		case 93: goto tr1084;
		case 114: goto st891;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
st891:
	if ( ++( p) == ( pe) )
		goto _test_eof891;
case 891:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 76: goto st838;
		case 93: goto tr1004;
		case 108: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st822;
tr2450:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st892;
st892:
	if ( ++( p) == ( pe) )
		goto _test_eof892;
case 892:
#line 21054 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st893;
		case 93: goto tr1086;
		case 114: goto st893;
	}
	goto st809;
st893:
	if ( ++( p) == ( pe) )
		goto _test_eof893;
case 893:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1087;
	}
	goto st809;
tr2451:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st894;
st894:
	if ( ++( p) == ( pe) )
		goto _test_eof894;
case 894:
#line 21083 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st895;
		case 93: goto tr989;
		case 111: goto st895;
	}
	goto st809;
st895:
	if ( ++( p) == ( pe) )
		goto _test_eof895;
case 895:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 68: goto st896;
		case 76: goto st903;
		case 93: goto tr989;
		case 100: goto st896;
		case 108: goto st903;
	}
	goto st809;
st896:
	if ( ++( p) == ( pe) )
		goto _test_eof896;
case 896:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st897;
		case 93: goto tr989;
		case 101: goto st897;
	}
	goto st809;
st897:
	if ( ++( p) == ( pe) )
		goto _test_eof897;
case 897:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st898;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st898;
		case 61: goto st899;
		case 93: goto tr1094;
	}
	goto st809;
st898:
	if ( ++( p) == ( pe) )
		goto _test_eof898;
case 898:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st898;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st898;
		case 61: goto st899;
		case 93: goto tr989;
	}
	goto st809;
st899:
	if ( ++( p) == ( pe) )
		goto _test_eof899;
case 899:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st899;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st899;
		case 93: goto tr989;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1095;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1095;
	} else
		goto tr1095;
	goto st809;
tr1095:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st900;
st900:
	if ( ++( p) == ( pe) )
		goto _test_eof900;
case 900:
#line 21178 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1097;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st900;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st900;
	} else
		goto st900;
	goto st809;
tr1097:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 503 "ext/dtext/dtext.cpp.rl"
	{( act) = 80;}
	goto st1921;
st1921:
	if ( ++( p) == ( pe) )
		goto _test_eof1921;
case 1921:
#line 21208 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1099;
		case 9: goto st901;
		case 10: goto tr1099;
		case 32: goto st901;
		case 40: goto st811;
	}
	goto tr2460;
st901:
	if ( ++( p) == ( pe) )
		goto _test_eof901;
case 901:
	switch( (*( p)) ) {
		case 0: goto tr1099;
		case 9: goto st901;
		case 10: goto tr1099;
		case 32: goto st901;
	}
	goto tr1098;
tr1094:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1922;
st1922:
	if ( ++( p) == ( pe) )
		goto _test_eof1922;
case 1922:
#line 21236 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1102;
		case 9: goto st902;
		case 10: goto tr1102;
		case 32: goto st902;
	}
	goto tr2461;
st902:
	if ( ++( p) == ( pe) )
		goto _test_eof902;
case 902:
	switch( (*( p)) ) {
		case 0: goto tr1102;
		case 9: goto st902;
		case 10: goto tr1102;
		case 32: goto st902;
	}
	goto tr1101;
st903:
	if ( ++( p) == ( pe) )
		goto _test_eof903;
case 903:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 71: goto st904;
		case 79: goto st909;
		case 93: goto tr261;
		case 103: goto st904;
		case 111: goto st909;
	}
	goto st809;
st904:
	if ( ++( p) == ( pe) )
		goto _test_eof904;
case 904:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st905;
		case 93: goto tr989;
		case 114: goto st905;
	}
	goto st809;
st905:
	if ( ++( p) == ( pe) )
		goto _test_eof905;
case 905:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st906;
		case 93: goto tr989;
		case 111: goto st906;
	}
	goto st809;
st906:
	if ( ++( p) == ( pe) )
		goto _test_eof906;
case 906:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 85: goto st907;
		case 93: goto tr989;
		case 117: goto st907;
	}
	goto st809;
st907:
	if ( ++( p) == ( pe) )
		goto _test_eof907;
case 907:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st908;
		case 93: goto tr989;
		case 112: goto st908;
	}
	goto st809;
st908:
	if ( ++( p) == ( pe) )
		goto _test_eof908;
case 908:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr261;
	}
	goto st809;
st909:
	if ( ++( p) == ( pe) )
		goto _test_eof909;
case 909:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st910;
		case 93: goto tr989;
		case 114: goto st910;
	}
	goto st809;
st910:
	if ( ++( p) == ( pe) )
		goto _test_eof910;
case 910:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 61: goto st911;
		case 93: goto tr989;
	}
	goto st809;
st911:
	if ( ++( p) == ( pe) )
		goto _test_eof911;
case 911:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 35: goto tr1112;
		case 65: goto tr1113;
		case 67: goto tr1114;
		case 69: goto tr1115;
		case 71: goto tr1116;
		case 73: goto tr1117;
		case 76: goto tr1118;
		case 77: goto tr1119;
		case 79: goto tr1120;
		case 81: goto tr1121;
		case 83: goto tr1122;
		case 86: goto tr1123;
		case 93: goto tr989;
		case 97: goto tr1124;
		case 99: goto tr1126;
		case 101: goto tr1127;
		case 103: goto tr1128;
		case 105: goto tr1129;
		case 108: goto tr1130;
		case 109: goto tr1131;
		case 111: goto tr1132;
		case 113: goto tr1133;
		case 115: goto tr1134;
		case 118: goto tr1135;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto tr1125;
	goto st809;
tr1112:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st912;
st912:
	if ( ++( p) == ( pe) )
		goto _test_eof912;
case 912:
#line 21402 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st913;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st913;
	} else
		goto st913;
	goto st809;
st913:
	if ( ++( p) == ( pe) )
		goto _test_eof913;
case 913:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st914;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st914;
	} else
		goto st914;
	goto st809;
st914:
	if ( ++( p) == ( pe) )
		goto _test_eof914;
case 914:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st915;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st915;
	} else
		goto st915;
	goto st809;
st915:
	if ( ++( p) == ( pe) )
		goto _test_eof915;
case 915:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1140;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st916;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st916;
	} else
		goto st916;
	goto st809;
st916:
	if ( ++( p) == ( pe) )
		goto _test_eof916;
case 916:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1140;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st917;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st917;
	} else
		goto st917;
	goto st809;
st917:
	if ( ++( p) == ( pe) )
		goto _test_eof917;
case 917:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1140;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st918;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st918;
	} else
		goto st918;
	goto st809;
st918:
	if ( ++( p) == ( pe) )
		goto _test_eof918;
case 918:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1140;
	}
	goto st809;
tr1140:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 471 "ext/dtext/dtext.cpp.rl"
	{( act) = 76;}
	goto st1923;
tr1146:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 463 "ext/dtext/dtext.cpp.rl"
	{( act) = 75;}
	goto st1923;
tr1324:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 438 "ext/dtext/dtext.cpp.rl"
	{( act) = 70;}
	goto st1923;
tr1325:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 436 "ext/dtext/dtext.cpp.rl"
	{( act) = 68;}
	goto st1923;
st1923:
	if ( ++( p) == ( pe) )
		goto _test_eof1923;
case 1923:
#line 21564 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st811;
	goto tr254;
tr1113:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st919;
st919:
	if ( ++( p) == ( pe) )
		goto _test_eof919;
case 919:
#line 21576 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st920;
		case 93: goto tr989;
		case 114: goto st920;
	}
	goto st809;
st920:
	if ( ++( p) == ( pe) )
		goto _test_eof920;
case 920:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st921;
		case 93: goto tr989;
		case 116: goto st921;
	}
	goto st809;
st921:
	if ( ++( p) == ( pe) )
		goto _test_eof921;
case 921:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st922;
		case 93: goto tr1146;
		case 105: goto st922;
	}
	goto st809;
st922:
	if ( ++( p) == ( pe) )
		goto _test_eof922;
case 922:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 83: goto st923;
		case 93: goto tr989;
		case 115: goto st923;
	}
	goto st809;
st923:
	if ( ++( p) == ( pe) )
		goto _test_eof923;
case 923:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st924;
		case 93: goto tr989;
		case 116: goto st924;
	}
	goto st809;
st924:
	if ( ++( p) == ( pe) )
		goto _test_eof924;
case 924:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1146;
	}
	goto st809;
tr1114:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st925;
st925:
	if ( ++( p) == ( pe) )
		goto _test_eof925;
case 925:
#line 21657 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 72: goto st926;
		case 79: goto st933;
		case 93: goto tr989;
		case 104: goto st926;
		case 111: goto st933;
	}
	goto st809;
st926:
	if ( ++( p) == ( pe) )
		goto _test_eof926;
case 926:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st927;
		case 93: goto tr1146;
		case 97: goto st927;
	}
	goto st809;
st927:
	if ( ++( p) == ( pe) )
		goto _test_eof927;
case 927:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st928;
		case 93: goto tr989;
		case 114: goto st928;
	}
	goto st809;
st928:
	if ( ++( p) == ( pe) )
		goto _test_eof928;
case 928:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st929;
		case 93: goto tr1146;
		case 97: goto st929;
	}
	goto st809;
st929:
	if ( ++( p) == ( pe) )
		goto _test_eof929;
case 929:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st930;
		case 93: goto tr989;
		case 99: goto st930;
	}
	goto st809;
st930:
	if ( ++( p) == ( pe) )
		goto _test_eof930;
case 930:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st931;
		case 93: goto tr989;
		case 116: goto st931;
	}
	goto st809;
st931:
	if ( ++( p) == ( pe) )
		goto _test_eof931;
case 931:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st932;
		case 93: goto tr989;
		case 101: goto st932;
	}
	goto st809;
st932:
	if ( ++( p) == ( pe) )
		goto _test_eof932;
case 932:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st924;
		case 93: goto tr989;
		case 114: goto st924;
	}
	goto st809;
st933:
	if ( ++( p) == ( pe) )
		goto _test_eof933;
case 933:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st934;
		case 93: goto tr1146;
		case 112: goto st934;
	}
	goto st809;
st934:
	if ( ++( p) == ( pe) )
		goto _test_eof934;
case 934:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 89: goto st935;
		case 93: goto tr989;
		case 121: goto st935;
	}
	goto st809;
st935:
	if ( ++( p) == ( pe) )
		goto _test_eof935;
case 935:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st936;
		case 93: goto tr1146;
		case 114: goto st936;
	}
	goto st809;
st936:
	if ( ++( p) == ( pe) )
		goto _test_eof936;
case 936:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st937;
		case 93: goto tr989;
		case 105: goto st937;
	}
	goto st809;
st937:
	if ( ++( p) == ( pe) )
		goto _test_eof937;
case 937:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 71: goto st938;
		case 93: goto tr989;
		case 103: goto st938;
	}
	goto st809;
st938:
	if ( ++( p) == ( pe) )
		goto _test_eof938;
case 938:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 72: goto st923;
		case 93: goto tr989;
		case 104: goto st923;
	}
	goto st809;
tr1115:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st939;
st939:
	if ( ++( p) == ( pe) )
		goto _test_eof939;
case 939:
#line 21846 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 88: goto st940;
		case 93: goto tr1146;
		case 120: goto st940;
	}
	goto st809;
st940:
	if ( ++( p) == ( pe) )
		goto _test_eof940;
case 940:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st941;
		case 93: goto tr989;
		case 112: goto st941;
	}
	goto st809;
st941:
	if ( ++( p) == ( pe) )
		goto _test_eof941;
case 941:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st942;
		case 93: goto tr989;
		case 108: goto st942;
	}
	goto st809;
st942:
	if ( ++( p) == ( pe) )
		goto _test_eof942;
case 942:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st943;
		case 93: goto tr989;
		case 105: goto st943;
	}
	goto st809;
st943:
	if ( ++( p) == ( pe) )
		goto _test_eof943;
case 943:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st944;
		case 93: goto tr989;
		case 99: goto st944;
	}
	goto st809;
st944:
	if ( ++( p) == ( pe) )
		goto _test_eof944;
case 944:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st923;
		case 93: goto tr989;
		case 105: goto st923;
	}
	goto st809;
tr1116:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st945;
st945:
	if ( ++( p) == ( pe) )
		goto _test_eof945;
case 945:
#line 21929 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st946;
		case 93: goto tr989;
		case 101: goto st946;
	}
	goto st809;
st946:
	if ( ++( p) == ( pe) )
		goto _test_eof946;
case 946:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 78: goto st947;
		case 93: goto tr989;
		case 110: goto st947;
	}
	goto st809;
st947:
	if ( ++( p) == ( pe) )
		goto _test_eof947;
case 947:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st948;
		case 93: goto tr1146;
		case 101: goto st948;
	}
	goto st809;
st948:
	if ( ++( p) == ( pe) )
		goto _test_eof948;
case 948:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st949;
		case 93: goto tr989;
		case 114: goto st949;
	}
	goto st809;
st949:
	if ( ++( p) == ( pe) )
		goto _test_eof949;
case 949:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st950;
		case 93: goto tr989;
		case 97: goto st950;
	}
	goto st809;
st950:
	if ( ++( p) == ( pe) )
		goto _test_eof950;
case 950:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st924;
		case 93: goto tr989;
		case 108: goto st924;
	}
	goto st809;
tr1117:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st951;
st951:
	if ( ++( p) == ( pe) )
		goto _test_eof951;
case 951:
#line 22012 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 78: goto st952;
		case 93: goto tr989;
		case 110: goto st952;
	}
	goto st809;
st952:
	if ( ++( p) == ( pe) )
		goto _test_eof952;
case 952:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 86: goto st953;
		case 93: goto tr989;
		case 118: goto st953;
	}
	goto st809;
st953:
	if ( ++( p) == ( pe) )
		goto _test_eof953;
case 953:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st954;
		case 93: goto tr1146;
		case 97: goto st954;
	}
	goto st809;
st954:
	if ( ++( p) == ( pe) )
		goto _test_eof954;
case 954:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st955;
		case 93: goto tr989;
		case 108: goto st955;
	}
	goto st809;
st955:
	if ( ++( p) == ( pe) )
		goto _test_eof955;
case 955:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st956;
		case 93: goto tr989;
		case 105: goto st956;
	}
	goto st809;
st956:
	if ( ++( p) == ( pe) )
		goto _test_eof956;
case 956:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 68: goto st924;
		case 93: goto tr989;
		case 100: goto st924;
	}
	goto st809;
tr1118:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st957;
st957:
	if ( ++( p) == ( pe) )
		goto _test_eof957;
case 957:
#line 22095 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st958;
		case 93: goto tr989;
		case 111: goto st958;
	}
	goto st809;
st958:
	if ( ++( p) == ( pe) )
		goto _test_eof958;
case 958:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st959;
		case 93: goto tr989;
		case 114: goto st959;
	}
	goto st809;
st959:
	if ( ++( p) == ( pe) )
		goto _test_eof959;
case 959:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st924;
		case 93: goto tr1146;
		case 101: goto st924;
	}
	goto st809;
tr1119:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st960;
st960:
	if ( ++( p) == ( pe) )
		goto _test_eof960;
case 960:
#line 22139 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st961;
		case 93: goto tr989;
		case 101: goto st961;
	}
	goto st809;
st961:
	if ( ++( p) == ( pe) )
		goto _test_eof961;
case 961:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st962;
		case 93: goto tr989;
		case 116: goto st962;
	}
	goto st809;
st962:
	if ( ++( p) == ( pe) )
		goto _test_eof962;
case 962:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st924;
		case 93: goto tr989;
		case 97: goto st924;
	}
	goto st809;
tr1120:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st963;
st963:
	if ( ++( p) == ( pe) )
		goto _test_eof963;
case 963:
#line 22183 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st924;
		case 93: goto tr989;
		case 99: goto st924;
	}
	goto st809;
tr1121:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st964;
st964:
	if ( ++( p) == ( pe) )
		goto _test_eof964;
case 964:
#line 22201 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 85: goto st965;
		case 93: goto tr1146;
		case 117: goto st965;
	}
	goto st809;
st965:
	if ( ++( p) == ( pe) )
		goto _test_eof965;
case 965:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st966;
		case 93: goto tr989;
		case 101: goto st966;
	}
	goto st809;
st966:
	if ( ++( p) == ( pe) )
		goto _test_eof966;
case 966:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 83: goto st967;
		case 93: goto tr989;
		case 115: goto st967;
	}
	goto st809;
st967:
	if ( ++( p) == ( pe) )
		goto _test_eof967;
case 967:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st968;
		case 93: goto tr989;
		case 116: goto st968;
	}
	goto st809;
st968:
	if ( ++( p) == ( pe) )
		goto _test_eof968;
case 968:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st969;
		case 93: goto tr989;
		case 105: goto st969;
	}
	goto st809;
st969:
	if ( ++( p) == ( pe) )
		goto _test_eof969;
case 969:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st970;
		case 93: goto tr989;
		case 111: goto st970;
	}
	goto st809;
st970:
	if ( ++( p) == ( pe) )
		goto _test_eof970;
case 970:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 78: goto st971;
		case 93: goto tr989;
		case 110: goto st971;
	}
	goto st809;
st971:
	if ( ++( p) == ( pe) )
		goto _test_eof971;
case 971:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st972;
		case 93: goto tr989;
		case 97: goto st972;
	}
	goto st809;
st972:
	if ( ++( p) == ( pe) )
		goto _test_eof972;
case 972:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 66: goto st973;
		case 93: goto tr989;
		case 98: goto st973;
	}
	goto st809;
st973:
	if ( ++( p) == ( pe) )
		goto _test_eof973;
case 973:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st974;
		case 93: goto tr989;
		case 108: goto st974;
	}
	goto st809;
st974:
	if ( ++( p) == ( pe) )
		goto _test_eof974;
case 974:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st924;
		case 93: goto tr989;
		case 101: goto st924;
	}
	goto st809;
tr1122:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st975;
st975:
	if ( ++( p) == ( pe) )
		goto _test_eof975;
case 975:
#line 22349 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st976;
		case 80: goto st977;
		case 93: goto tr1146;
		case 97: goto st976;
		case 112: goto st977;
	}
	goto st809;
st976:
	if ( ++( p) == ( pe) )
		goto _test_eof976;
case 976:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 70: goto st974;
		case 93: goto tr989;
		case 102: goto st974;
	}
	goto st809;
st977:
	if ( ++( p) == ( pe) )
		goto _test_eof977;
case 977:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st978;
		case 93: goto tr989;
		case 101: goto st978;
	}
	goto st809;
st978:
	if ( ++( p) == ( pe) )
		goto _test_eof978;
case 978:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st979;
		case 93: goto tr989;
		case 99: goto st979;
	}
	goto st809;
st979:
	if ( ++( p) == ( pe) )
		goto _test_eof979;
case 979:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st980;
		case 93: goto tr1146;
		case 105: goto st980;
	}
	goto st809;
st980:
	if ( ++( p) == ( pe) )
		goto _test_eof980;
case 980:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st981;
		case 93: goto tr989;
		case 101: goto st981;
	}
	goto st809;
st981:
	if ( ++( p) == ( pe) )
		goto _test_eof981;
case 981:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 83: goto st924;
		case 93: goto tr989;
		case 115: goto st924;
	}
	goto st809;
tr1123:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st982;
st982:
	if ( ++( p) == ( pe) )
		goto _test_eof982;
case 982:
#line 22447 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st983;
		case 93: goto tr989;
		case 111: goto st983;
	}
	goto st809;
st983:
	if ( ++( p) == ( pe) )
		goto _test_eof983;
case 983:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st984;
		case 93: goto tr989;
		case 105: goto st984;
	}
	goto st809;
st984:
	if ( ++( p) == ( pe) )
		goto _test_eof984;
case 984:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st985;
		case 93: goto tr989;
		case 99: goto st985;
	}
	goto st809;
st985:
	if ( ++( p) == ( pe) )
		goto _test_eof985;
case 985:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st986;
		case 93: goto tr989;
		case 101: goto st986;
	}
	goto st809;
st986:
	if ( ++( p) == ( pe) )
		goto _test_eof986;
case 986:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 45: goto st987;
		case 93: goto tr989;
	}
	goto st809;
st987:
	if ( ++( p) == ( pe) )
		goto _test_eof987;
case 987:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st988;
		case 93: goto tr989;
		case 97: goto st988;
	}
	goto st809;
st988:
	if ( ++( p) == ( pe) )
		goto _test_eof988;
case 988:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st989;
		case 93: goto tr989;
		case 99: goto st989;
	}
	goto st809;
st989:
	if ( ++( p) == ( pe) )
		goto _test_eof989;
case 989:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st990;
		case 93: goto tr989;
		case 116: goto st990;
	}
	goto st809;
st990:
	if ( ++( p) == ( pe) )
		goto _test_eof990;
case 990:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st932;
		case 93: goto tr989;
		case 111: goto st932;
	}
	goto st809;
tr1124:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st991;
st991:
	if ( ++( p) == ( pe) )
		goto _test_eof991;
case 991:
#line 22568 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st920;
		case 93: goto tr1140;
		case 114: goto st993;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1125:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st992;
st992:
	if ( ++( p) == ( pe) )
		goto _test_eof992;
case 992:
#line 22588 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1140;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st993:
	if ( ++( p) == ( pe) )
		goto _test_eof993;
case 993:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st921;
		case 93: goto tr1140;
		case 116: goto st994;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st994:
	if ( ++( p) == ( pe) )
		goto _test_eof994;
case 994:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st922;
		case 93: goto tr1146;
		case 105: goto st995;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st995:
	if ( ++( p) == ( pe) )
		goto _test_eof995;
case 995:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 83: goto st923;
		case 93: goto tr1140;
		case 115: goto st996;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st996:
	if ( ++( p) == ( pe) )
		goto _test_eof996;
case 996:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st924;
		case 93: goto tr1140;
		case 116: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st997:
	if ( ++( p) == ( pe) )
		goto _test_eof997;
case 997:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1146;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1126:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st998;
st998:
	if ( ++( p) == ( pe) )
		goto _test_eof998;
case 998:
#line 22679 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 72: goto st926;
		case 79: goto st933;
		case 93: goto tr1140;
		case 104: goto st999;
		case 111: goto st1006;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st999:
	if ( ++( p) == ( pe) )
		goto _test_eof999;
case 999:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st927;
		case 93: goto tr1146;
		case 97: goto st1000;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1000:
	if ( ++( p) == ( pe) )
		goto _test_eof1000;
case 1000:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st928;
		case 93: goto tr1140;
		case 114: goto st1001;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1001:
	if ( ++( p) == ( pe) )
		goto _test_eof1001;
case 1001:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st929;
		case 93: goto tr1146;
		case 97: goto st1002;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1002:
	if ( ++( p) == ( pe) )
		goto _test_eof1002;
case 1002:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st930;
		case 93: goto tr1140;
		case 99: goto st1003;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1003:
	if ( ++( p) == ( pe) )
		goto _test_eof1003;
case 1003:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st931;
		case 93: goto tr1140;
		case 116: goto st1004;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1004:
	if ( ++( p) == ( pe) )
		goto _test_eof1004;
case 1004:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st932;
		case 93: goto tr1140;
		case 101: goto st1005;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1005:
	if ( ++( p) == ( pe) )
		goto _test_eof1005;
case 1005:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st924;
		case 93: goto tr1140;
		case 114: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1006:
	if ( ++( p) == ( pe) )
		goto _test_eof1006;
case 1006:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st934;
		case 93: goto tr1146;
		case 112: goto st1007;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1007:
	if ( ++( p) == ( pe) )
		goto _test_eof1007;
case 1007:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 89: goto st935;
		case 93: goto tr1140;
		case 121: goto st1008;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1008:
	if ( ++( p) == ( pe) )
		goto _test_eof1008;
case 1008:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st936;
		case 93: goto tr1146;
		case 114: goto st1009;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1009:
	if ( ++( p) == ( pe) )
		goto _test_eof1009;
case 1009:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st937;
		case 93: goto tr1140;
		case 105: goto st1010;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1010:
	if ( ++( p) == ( pe) )
		goto _test_eof1010;
case 1010:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 71: goto st938;
		case 93: goto tr1140;
		case 103: goto st1011;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1011:
	if ( ++( p) == ( pe) )
		goto _test_eof1011;
case 1011:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 72: goto st923;
		case 93: goto tr1140;
		case 104: goto st996;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1127:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1012;
st1012:
	if ( ++( p) == ( pe) )
		goto _test_eof1012;
case 1012:
#line 22896 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 88: goto st940;
		case 93: goto tr1146;
		case 120: goto st1013;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1013:
	if ( ++( p) == ( pe) )
		goto _test_eof1013;
case 1013:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st941;
		case 93: goto tr1140;
		case 112: goto st1014;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1014:
	if ( ++( p) == ( pe) )
		goto _test_eof1014;
case 1014:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st942;
		case 93: goto tr1140;
		case 108: goto st1015;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1015:
	if ( ++( p) == ( pe) )
		goto _test_eof1015;
case 1015:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st943;
		case 93: goto tr1140;
		case 105: goto st1016;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1016:
	if ( ++( p) == ( pe) )
		goto _test_eof1016;
case 1016:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st944;
		case 93: goto tr1140;
		case 99: goto st1017;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1017:
	if ( ++( p) == ( pe) )
		goto _test_eof1017;
case 1017:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st923;
		case 93: goto tr1140;
		case 105: goto st996;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1128:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1018;
st1018:
	if ( ++( p) == ( pe) )
		goto _test_eof1018;
case 1018:
#line 22991 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st946;
		case 93: goto tr1140;
		case 101: goto st1019;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1019:
	if ( ++( p) == ( pe) )
		goto _test_eof1019;
case 1019:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 78: goto st947;
		case 93: goto tr1140;
		case 110: goto st1020;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1020:
	if ( ++( p) == ( pe) )
		goto _test_eof1020;
case 1020:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st948;
		case 93: goto tr1146;
		case 101: goto st1021;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1021:
	if ( ++( p) == ( pe) )
		goto _test_eof1021;
case 1021:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st949;
		case 93: goto tr1140;
		case 114: goto st1022;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1022:
	if ( ++( p) == ( pe) )
		goto _test_eof1022;
case 1022:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st950;
		case 93: goto tr1140;
		case 97: goto st1023;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1023:
	if ( ++( p) == ( pe) )
		goto _test_eof1023;
case 1023:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st924;
		case 93: goto tr1140;
		case 108: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1129:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1024;
st1024:
	if ( ++( p) == ( pe) )
		goto _test_eof1024;
case 1024:
#line 23086 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 78: goto st952;
		case 93: goto tr1140;
		case 110: goto st1025;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1025:
	if ( ++( p) == ( pe) )
		goto _test_eof1025;
case 1025:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 86: goto st953;
		case 93: goto tr1140;
		case 118: goto st1026;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1026:
	if ( ++( p) == ( pe) )
		goto _test_eof1026;
case 1026:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st954;
		case 93: goto tr1146;
		case 97: goto st1027;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1027:
	if ( ++( p) == ( pe) )
		goto _test_eof1027;
case 1027:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st955;
		case 93: goto tr1140;
		case 108: goto st1028;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1028:
	if ( ++( p) == ( pe) )
		goto _test_eof1028;
case 1028:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st956;
		case 93: goto tr1140;
		case 105: goto st1029;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1029:
	if ( ++( p) == ( pe) )
		goto _test_eof1029;
case 1029:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 68: goto st924;
		case 93: goto tr1140;
		case 100: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1130:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1030;
st1030:
	if ( ++( p) == ( pe) )
		goto _test_eof1030;
case 1030:
#line 23181 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st958;
		case 93: goto tr1140;
		case 111: goto st1031;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1031:
	if ( ++( p) == ( pe) )
		goto _test_eof1031;
case 1031:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st959;
		case 93: goto tr1140;
		case 114: goto st1032;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1032:
	if ( ++( p) == ( pe) )
		goto _test_eof1032;
case 1032:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st924;
		case 93: goto tr1146;
		case 101: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1131:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1033;
st1033:
	if ( ++( p) == ( pe) )
		goto _test_eof1033;
case 1033:
#line 23231 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st961;
		case 93: goto tr1140;
		case 101: goto st1034;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1034:
	if ( ++( p) == ( pe) )
		goto _test_eof1034;
case 1034:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st962;
		case 93: goto tr1140;
		case 116: goto st1035;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1035:
	if ( ++( p) == ( pe) )
		goto _test_eof1035;
case 1035:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st924;
		case 93: goto tr1140;
		case 97: goto st997;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1132:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1036;
st1036:
	if ( ++( p) == ( pe) )
		goto _test_eof1036;
case 1036:
#line 23281 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st924;
		case 93: goto tr1140;
		case 99: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1133:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1037;
st1037:
	if ( ++( p) == ( pe) )
		goto _test_eof1037;
case 1037:
#line 23301 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 85: goto st965;
		case 93: goto tr1146;
		case 117: goto st1038;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1038:
	if ( ++( p) == ( pe) )
		goto _test_eof1038;
case 1038:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st966;
		case 93: goto tr1140;
		case 101: goto st1039;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1039:
	if ( ++( p) == ( pe) )
		goto _test_eof1039;
case 1039:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 83: goto st967;
		case 93: goto tr1140;
		case 115: goto st1040;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1040:
	if ( ++( p) == ( pe) )
		goto _test_eof1040;
case 1040:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st968;
		case 93: goto tr1140;
		case 116: goto st1041;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1041:
	if ( ++( p) == ( pe) )
		goto _test_eof1041;
case 1041:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st969;
		case 93: goto tr1140;
		case 105: goto st1042;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1042:
	if ( ++( p) == ( pe) )
		goto _test_eof1042;
case 1042:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st970;
		case 93: goto tr1140;
		case 111: goto st1043;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1043:
	if ( ++( p) == ( pe) )
		goto _test_eof1043;
case 1043:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 78: goto st971;
		case 93: goto tr1140;
		case 110: goto st1044;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1044:
	if ( ++( p) == ( pe) )
		goto _test_eof1044;
case 1044:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st972;
		case 93: goto tr1140;
		case 97: goto st1045;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1045:
	if ( ++( p) == ( pe) )
		goto _test_eof1045;
case 1045:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 66: goto st973;
		case 93: goto tr1140;
		case 98: goto st1046;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1046:
	if ( ++( p) == ( pe) )
		goto _test_eof1046;
case 1046:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st974;
		case 93: goto tr1140;
		case 108: goto st1047;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1047:
	if ( ++( p) == ( pe) )
		goto _test_eof1047;
case 1047:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st924;
		case 93: goto tr1140;
		case 101: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1134:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1048;
st1048:
	if ( ++( p) == ( pe) )
		goto _test_eof1048;
case 1048:
#line 23471 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st976;
		case 80: goto st977;
		case 93: goto tr1146;
		case 97: goto st1049;
		case 112: goto st1050;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1049:
	if ( ++( p) == ( pe) )
		goto _test_eof1049;
case 1049:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 70: goto st974;
		case 93: goto tr1140;
		case 102: goto st1047;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1050:
	if ( ++( p) == ( pe) )
		goto _test_eof1050;
case 1050:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st978;
		case 93: goto tr1140;
		case 101: goto st1051;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1051:
	if ( ++( p) == ( pe) )
		goto _test_eof1051;
case 1051:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st979;
		case 93: goto tr1140;
		case 99: goto st1052;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1052:
	if ( ++( p) == ( pe) )
		goto _test_eof1052;
case 1052:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st980;
		case 93: goto tr1146;
		case 105: goto st1053;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1053:
	if ( ++( p) == ( pe) )
		goto _test_eof1053;
case 1053:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st981;
		case 93: goto tr1140;
		case 101: goto st1054;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1054:
	if ( ++( p) == ( pe) )
		goto _test_eof1054;
case 1054:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 83: goto st924;
		case 93: goto tr1140;
		case 115: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr1135:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1055;
st1055:
	if ( ++( p) == ( pe) )
		goto _test_eof1055;
case 1055:
#line 23583 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st983;
		case 93: goto tr1140;
		case 97: goto st997;
		case 111: goto st1056;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1056:
	if ( ++( p) == ( pe) )
		goto _test_eof1056;
case 1056:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st984;
		case 93: goto tr1140;
		case 105: goto st1057;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1057:
	if ( ++( p) == ( pe) )
		goto _test_eof1057;
case 1057:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st985;
		case 93: goto tr1140;
		case 99: goto st1058;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1058:
	if ( ++( p) == ( pe) )
		goto _test_eof1058;
case 1058:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st986;
		case 93: goto tr1140;
		case 101: goto st1059;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
st1059:
	if ( ++( p) == ( pe) )
		goto _test_eof1059;
case 1059:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 45: goto st987;
		case 93: goto tr1140;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st992;
	goto st809;
tr2452:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1060;
st1060:
	if ( ++( p) == ( pe) )
		goto _test_eof1060;
case 1060:
#line 23665 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st908;
		case 84: goto st1061;
		case 93: goto tr989;
		case 114: goto st908;
		case 116: goto st1061;
	}
	goto st809;
st1061:
	if ( ++( p) == ( pe) )
		goto _test_eof1061;
case 1061:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1062;
		case 93: goto tr989;
		case 116: goto st1062;
	}
	goto st809;
st1062:
	if ( ++( p) == ( pe) )
		goto _test_eof1062;
case 1062:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st1063;
		case 93: goto tr989;
		case 112: goto st1063;
	}
	goto st809;
st1063:
	if ( ++( p) == ( pe) )
		goto _test_eof1063;
case 1063:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 58: goto st1064;
		case 83: goto st1085;
		case 93: goto tr989;
		case 115: goto st1085;
	}
	goto st809;
st1064:
	if ( ++( p) == ( pe) )
		goto _test_eof1064;
case 1064:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1065;
		case 93: goto tr989;
	}
	goto st809;
st1065:
	if ( ++( p) == ( pe) )
		goto _test_eof1065;
case 1065:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1066;
		case 93: goto tr989;
	}
	goto st809;
st1066:
	if ( ++( p) == ( pe) )
		goto _test_eof1066;
case 1066:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1271;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1067;
st1067:
	if ( ++( p) == ( pe) )
		goto _test_eof1067;
case 1067:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1272;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1067;
tr1272:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1068;
st1068:
	if ( ++( p) == ( pe) )
		goto _test_eof1068;
case 1068:
#line 23779 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 40: goto st824;
		case 93: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1069:
	if ( ++( p) == ( pe) )
		goto _test_eof1069;
case 1069:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 32: goto tr254;
		case 93: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st1069;
tr1274:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1070;
st1070:
	if ( ++( p) == ( pe) )
		goto _test_eof1070;
case 1070:
#line 23809 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 32: goto tr254;
		case 40: goto st1071;
		case 93: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st1069;
st1071:
	if ( ++( p) == ( pe) )
		goto _test_eof1071;
case 1071:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 10: goto tr254;
		case 13: goto tr254;
	}
	goto tr1006;
tr1271:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1072;
st1072:
	if ( ++( p) == ( pe) )
		goto _test_eof1072;
case 1072:
#line 23837 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 40: goto st1073;
		case 93: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1073:
	if ( ++( p) == ( pe) )
		goto _test_eof1073;
case 1073:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 35: goto tr1277;
		case 47: goto tr1277;
		case 72: goto tr1278;
		case 93: goto tr1274;
		case 104: goto tr1278;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
tr1277:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1074;
st1074:
	if ( ++( p) == ( pe) )
		goto _test_eof1074;
case 1074:
#line 23871 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 41: goto tr1280;
		case 93: goto tr1281;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1074;
tr1280:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1924;
st1924:
	if ( ++( p) == ( pe) )
		goto _test_eof1924;
case 1924:
#line 23893 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2462;
		case 32: goto tr2462;
		case 93: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2462;
	goto st1069;
tr1281:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1075;
st1075:
	if ( ++( p) == ( pe) )
		goto _test_eof1075;
case 1075:
#line 23910 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 40: goto st1076;
		case 41: goto tr1280;
		case 93: goto tr1281;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1074;
st1076:
	if ( ++( p) == ( pe) )
		goto _test_eof1076;
case 1076:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1006;
		case 41: goto tr1284;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto tr1006;
	goto tr1283;
tr1284:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1925;
st1925:
	if ( ++( p) == ( pe) )
		goto _test_eof1925;
case 1925:
#line 23949 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2462;
		case 10: goto tr2462;
		case 13: goto tr2462;
		case 41: goto tr1010;
	}
	goto st825;
tr1278:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1077;
st1077:
	if ( ++( p) == ( pe) )
		goto _test_eof1077;
case 1077:
#line 23965 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 84: goto st1078;
		case 93: goto tr1274;
		case 116: goto st1078;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1078:
	if ( ++( p) == ( pe) )
		goto _test_eof1078;
case 1078:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 84: goto st1079;
		case 93: goto tr1274;
		case 116: goto st1079;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1079:
	if ( ++( p) == ( pe) )
		goto _test_eof1079;
case 1079:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 80: goto st1080;
		case 93: goto tr1274;
		case 112: goto st1080;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1080:
	if ( ++( p) == ( pe) )
		goto _test_eof1080;
case 1080:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 58: goto st1081;
		case 83: goto st1084;
		case 93: goto tr1274;
		case 115: goto st1084;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1081:
	if ( ++( p) == ( pe) )
		goto _test_eof1081;
case 1081:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 47: goto st1082;
		case 93: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1082:
	if ( ++( p) == ( pe) )
		goto _test_eof1082;
case 1082:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 47: goto st1083;
		case 93: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1083:
	if ( ++( p) == ( pe) )
		goto _test_eof1083;
case 1083:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 93: goto tr1281;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1074;
st1084:
	if ( ++( p) == ( pe) )
		goto _test_eof1084;
case 1084:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 58: goto st1081;
		case 93: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1069;
st1085:
	if ( ++( p) == ( pe) )
		goto _test_eof1085;
case 1085:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 58: goto st1064;
		case 93: goto tr989;
	}
	goto st809;
tr2453:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1086;
st1086:
	if ( ++( p) == ( pe) )
		goto _test_eof1086;
case 1086:
#line 24090 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1292;
	}
	goto st809;
tr2454:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1087;
st1087:
	if ( ++( p) == ( pe) )
		goto _test_eof1087;
case 1087:
#line 24106 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st1088;
		case 93: goto tr989;
		case 111: goto st1088;
	}
	goto st809;
st1088:
	if ( ++( p) == ( pe) )
		goto _test_eof1088;
case 1088:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 68: goto st1089;
		case 93: goto tr989;
		case 100: goto st1089;
	}
	goto st809;
st1089:
	if ( ++( p) == ( pe) )
		goto _test_eof1089;
case 1089:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1090;
		case 93: goto tr989;
		case 116: goto st1090;
	}
	goto st809;
st1090:
	if ( ++( p) == ( pe) )
		goto _test_eof1090;
case 1090:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st1091;
		case 93: goto tr989;
		case 101: goto st1091;
	}
	goto st809;
st1091:
	if ( ++( p) == ( pe) )
		goto _test_eof1091;
case 1091:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 88: goto st1092;
		case 93: goto tr989;
		case 120: goto st1092;
	}
	goto st809;
st1092:
	if ( ++( p) == ( pe) )
		goto _test_eof1092;
case 1092:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1093;
		case 93: goto tr989;
		case 116: goto st1093;
	}
	goto st809;
st1093:
	if ( ++( p) == ( pe) )
		goto _test_eof1093;
case 1093:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1299;
	}
	goto st809;
tr1299:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1926;
st1926:
	if ( ++( p) == ( pe) )
		goto _test_eof1926;
case 1926:
#line 24200 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1301;
		case 9: goto st1094;
		case 10: goto tr1301;
		case 32: goto st1094;
	}
	goto tr2463;
st1094:
	if ( ++( p) == ( pe) )
		goto _test_eof1094;
case 1094:
	switch( (*( p)) ) {
		case 0: goto tr1301;
		case 9: goto st1094;
		case 10: goto tr1301;
		case 32: goto st1094;
	}
	goto tr1300;
tr2455:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1095;
st1095:
	if ( ++( p) == ( pe) )
		goto _test_eof1095;
case 1095:
#line 24227 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 85: goto st1096;
		case 93: goto tr989;
		case 117: goto st1096;
	}
	goto st809;
st1096:
	if ( ++( p) == ( pe) )
		goto _test_eof1096;
case 1096:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st1097;
		case 93: goto tr989;
		case 111: goto st1097;
	}
	goto st809;
st1097:
	if ( ++( p) == ( pe) )
		goto _test_eof1097;
case 1097:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1098;
		case 93: goto tr989;
		case 116: goto st1098;
	}
	goto st809;
st1098:
	if ( ++( p) == ( pe) )
		goto _test_eof1098;
case 1098:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st1099;
		case 93: goto tr989;
		case 101: goto st1099;
	}
	goto st809;
st1099:
	if ( ++( p) == ( pe) )
		goto _test_eof1099;
case 1099:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr309;
	}
	goto st809;
tr2456:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1100;
st1100:
	if ( ++( p) == ( pe) )
		goto _test_eof1100;
case 1100:
#line 24295 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st1101;
		case 80: goto st1106;
		case 85: goto st1113;
		case 93: goto tr1310;
		case 101: goto st1101;
		case 112: goto st1106;
		case 117: goto st1113;
	}
	goto st809;
st1101:
	if ( ++( p) == ( pe) )
		goto _test_eof1101;
case 1101:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 67: goto st1102;
		case 93: goto tr989;
		case 99: goto st1102;
	}
	goto st809;
st1102:
	if ( ++( p) == ( pe) )
		goto _test_eof1102;
case 1102:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1103;
		case 93: goto tr989;
		case 116: goto st1103;
	}
	goto st809;
st1103:
	if ( ++( p) == ( pe) )
		goto _test_eof1103;
case 1103:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st1104;
		case 93: goto tr989;
		case 105: goto st1104;
	}
	goto st809;
st1104:
	if ( ++( p) == ( pe) )
		goto _test_eof1104;
case 1104:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st1105;
		case 93: goto tr989;
		case 111: goto st1105;
	}
	goto st809;
st1105:
	if ( ++( p) == ( pe) )
		goto _test_eof1105;
case 1105:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 78: goto st908;
		case 93: goto tr989;
		case 110: goto st908;
	}
	goto st809;
st1106:
	if ( ++( p) == ( pe) )
		goto _test_eof1106;
case 1106:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st1107;
		case 93: goto tr989;
		case 111: goto st1107;
	}
	goto st809;
st1107:
	if ( ++( p) == ( pe) )
		goto _test_eof1107;
case 1107:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 73: goto st1108;
		case 93: goto tr989;
		case 105: goto st1108;
	}
	goto st809;
st1108:
	if ( ++( p) == ( pe) )
		goto _test_eof1108;
case 1108:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st1109;
		case 93: goto tr989;
		case 108: goto st1109;
	}
	goto st809;
st1109:
	if ( ++( p) == ( pe) )
		goto _test_eof1109;
case 1109:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st1110;
		case 93: goto tr989;
		case 101: goto st1110;
	}
	goto st809;
st1110:
	if ( ++( p) == ( pe) )
		goto _test_eof1110;
case 1110:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st1111;
		case 93: goto tr989;
		case 114: goto st1111;
	}
	goto st809;
st1111:
	if ( ++( p) == ( pe) )
		goto _test_eof1111;
case 1111:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 83: goto st1112;
		case 93: goto tr1321;
		case 115: goto st1112;
	}
	goto st809;
st1112:
	if ( ++( p) == ( pe) )
		goto _test_eof1112;
case 1112:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1321;
	}
	goto st809;
st1113:
	if ( ++( p) == ( pe) )
		goto _test_eof1113;
case 1113:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 66: goto st1114;
		case 80: goto st1115;
		case 93: goto tr989;
		case 98: goto st1114;
		case 112: goto st1115;
	}
	goto st809;
st1114:
	if ( ++( p) == ( pe) )
		goto _test_eof1114;
case 1114:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1324;
	}
	goto st809;
st1115:
	if ( ++( p) == ( pe) )
		goto _test_eof1115;
case 1115:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1325;
	}
	goto st809;
tr2457:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1116;
st1116:
	if ( ++( p) == ( pe) )
		goto _test_eof1116;
case 1116:
#line 24508 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st1117;
		case 66: goto st1120;
		case 68: goto st908;
		case 72: goto st1123;
		case 78: goto st1126;
		case 82: goto st908;
		case 93: goto tr989;
		case 97: goto st1117;
		case 98: goto st1120;
		case 100: goto st908;
		case 104: goto st1123;
		case 110: goto st1126;
		case 114: goto st908;
	}
	goto st809;
st1117:
	if ( ++( p) == ( pe) )
		goto _test_eof1117;
case 1117:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 66: goto st1118;
		case 93: goto tr989;
		case 98: goto st1118;
	}
	goto st809;
st1118:
	if ( ++( p) == ( pe) )
		goto _test_eof1118;
case 1118:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st1119;
		case 93: goto tr989;
		case 108: goto st1119;
	}
	goto st809;
st1119:
	if ( ++( p) == ( pe) )
		goto _test_eof1119;
case 1119:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st908;
		case 93: goto tr989;
		case 101: goto st908;
	}
	goto st809;
st1120:
	if ( ++( p) == ( pe) )
		goto _test_eof1120;
case 1120:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 79: goto st1121;
		case 93: goto tr989;
		case 111: goto st1121;
	}
	goto st809;
st1121:
	if ( ++( p) == ( pe) )
		goto _test_eof1121;
case 1121:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 68: goto st1122;
		case 93: goto tr989;
		case 100: goto st1122;
	}
	goto st809;
st1122:
	if ( ++( p) == ( pe) )
		goto _test_eof1122;
case 1122:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 89: goto st908;
		case 93: goto tr989;
		case 121: goto st908;
	}
	goto st809;
st1123:
	if ( ++( p) == ( pe) )
		goto _test_eof1123;
case 1123:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 69: goto st1124;
		case 93: goto tr261;
		case 101: goto st1124;
	}
	goto st809;
st1124:
	if ( ++( p) == ( pe) )
		goto _test_eof1124;
case 1124:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 65: goto st1125;
		case 93: goto tr989;
		case 97: goto st1125;
	}
	goto st809;
st1125:
	if ( ++( p) == ( pe) )
		goto _test_eof1125;
case 1125:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 68: goto st908;
		case 93: goto tr989;
		case 100: goto st908;
	}
	goto st809;
st1126:
	if ( ++( p) == ( pe) )
		goto _test_eof1126;
case 1126:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr1336;
	}
	goto st809;
tr2458:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1127;
st1127:
	if ( ++( p) == ( pe) )
		goto _test_eof1127;
case 1127:
#line 24664 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 82: goto st1128;
		case 93: goto tr1338;
		case 114: goto st1128;
	}
	goto st809;
st1128:
	if ( ++( p) == ( pe) )
		goto _test_eof1128;
case 1128:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 76: goto st1129;
		case 93: goto tr989;
		case 108: goto st1129;
	}
	goto st809;
st1129:
	if ( ++( p) == ( pe) )
		goto _test_eof1129;
case 1129:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1130;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1130;
		case 61: goto st1131;
		case 93: goto st1225;
	}
	goto st809;
st1130:
	if ( ++( p) == ( pe) )
		goto _test_eof1130;
case 1130:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1130;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1130;
		case 61: goto st1131;
		case 93: goto tr989;
	}
	goto st809;
st1131:
	if ( ++( p) == ( pe) )
		goto _test_eof1131;
case 1131:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1131;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1131;
		case 34: goto st1132;
		case 35: goto tr1344;
		case 39: goto st1183;
		case 47: goto tr1344;
		case 72: goto tr1346;
		case 93: goto tr989;
		case 104: goto tr1346;
	}
	goto st809;
st1132:
	if ( ++( p) == ( pe) )
		goto _test_eof1132;
case 1132:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 35: goto tr1347;
		case 47: goto tr1347;
		case 72: goto tr1348;
		case 93: goto tr989;
		case 104: goto tr1348;
	}
	goto st809;
tr1347:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1133;
st1133:
	if ( ++( p) == ( pe) )
		goto _test_eof1133;
case 1133:
#line 24757 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 34: goto tr1350;
		case 93: goto tr1351;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1133;
tr1350:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1134;
st1134:
	if ( ++( p) == ( pe) )
		goto _test_eof1134;
case 1134:
#line 24777 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1134;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1134;
		case 93: goto tr1353;
	}
	goto st809;
tr1353:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1135;
tr1411:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1135;
st1135:
	if ( ++( p) == ( pe) )
		goto _test_eof1135;
case 1135:
#line 24801 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1143;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1143;
		case 40: goto tr1356;
	}
	goto tr1354;
tr1354:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1136;
st1136:
	if ( ++( p) == ( pe) )
		goto _test_eof1136;
case 1136:
#line 24819 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr1358;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr1358;
		case 91: goto tr1359;
	}
	goto st1136;
tr1358:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1137;
st1137:
	if ( ++( p) == ( pe) )
		goto _test_eof1137;
case 1137:
#line 24837 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto st1137;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto st1137;
		case 91: goto st1138;
	}
	goto st1136;
tr1359:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1138;
st1138:
	if ( ++( p) == ( pe) )
		goto _test_eof1138;
case 1138:
#line 24855 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr1358;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr1358;
		case 47: goto st1139;
		case 91: goto tr1359;
	}
	goto st1136;
st1139:
	if ( ++( p) == ( pe) )
		goto _test_eof1139;
case 1139:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr1358;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr1358;
		case 85: goto st1140;
		case 91: goto tr1359;
		case 117: goto st1140;
	}
	goto st1136;
st1140:
	if ( ++( p) == ( pe) )
		goto _test_eof1140;
case 1140:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr1358;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr1358;
		case 82: goto st1141;
		case 91: goto tr1359;
		case 114: goto st1141;
	}
	goto st1136;
st1141:
	if ( ++( p) == ( pe) )
		goto _test_eof1141;
case 1141:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr1358;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr1358;
		case 76: goto st1142;
		case 91: goto tr1359;
		case 108: goto st1142;
	}
	goto st1136;
st1142:
	if ( ++( p) == ( pe) )
		goto _test_eof1142;
case 1142:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr1358;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto tr1358;
		case 91: goto tr1359;
		case 93: goto tr1366;
	}
	goto st1136;
tr1446:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1143;
st1143:
	if ( ++( p) == ( pe) )
		goto _test_eof1143;
case 1143:
#line 24933 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto st1143;
		case 10: goto tr254;
		case 13: goto tr254;
		case 32: goto st1143;
	}
	goto tr1354;
tr1356:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1144;
st1144:
	if ( ++( p) == ( pe) )
		goto _test_eof1144;
case 1144:
#line 24950 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 35: goto tr1367;
		case 47: goto tr1367;
		case 72: goto tr1368;
		case 91: goto tr1359;
		case 104: goto tr1368;
	}
	goto st1136;
tr1394:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1145;
tr1367:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1145;
st1145:
	if ( ++( p) == ( pe) )
		goto _test_eof1145;
case 1145:
#line 24976 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 41: goto tr1370;
		case 91: goto tr1371;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1136;
	goto st1145;
tr1370:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1927;
tr1395:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1927;
st1927:
	if ( ++( p) == ( pe) )
		goto _test_eof1927;
case 1927:
#line 25011 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2462;
		case 9: goto tr1358;
		case 10: goto tr2462;
		case 13: goto tr2462;
		case 32: goto tr1358;
		case 91: goto tr1359;
	}
	goto st1136;
tr1371:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1146;
st1146:
	if ( ++( p) == ( pe) )
		goto _test_eof1146;
case 1146:
#line 25029 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 41: goto tr1370;
		case 47: goto st1147;
		case 91: goto tr1371;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1136;
	goto st1145;
st1147:
	if ( ++( p) == ( pe) )
		goto _test_eof1147;
case 1147:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 41: goto tr1370;
		case 85: goto st1148;
		case 91: goto tr1371;
		case 117: goto st1148;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1136;
	goto st1145;
st1148:
	if ( ++( p) == ( pe) )
		goto _test_eof1148;
case 1148:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 41: goto tr1370;
		case 82: goto st1149;
		case 91: goto tr1371;
		case 114: goto st1149;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1136;
	goto st1145;
st1149:
	if ( ++( p) == ( pe) )
		goto _test_eof1149;
case 1149:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 41: goto tr1370;
		case 76: goto st1150;
		case 91: goto tr1371;
		case 108: goto st1150;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1136;
	goto st1145;
st1150:
	if ( ++( p) == ( pe) )
		goto _test_eof1150;
case 1150:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 41: goto tr1370;
		case 91: goto tr1371;
		case 93: goto tr1376;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1136;
	goto st1145;
tr1368:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1151;
st1151:
	if ( ++( p) == ( pe) )
		goto _test_eof1151;
case 1151:
#line 25122 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 84: goto st1152;
		case 91: goto tr1359;
		case 116: goto st1152;
	}
	goto st1136;
st1152:
	if ( ++( p) == ( pe) )
		goto _test_eof1152;
case 1152:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 84: goto st1153;
		case 91: goto tr1359;
		case 116: goto st1153;
	}
	goto st1136;
st1153:
	if ( ++( p) == ( pe) )
		goto _test_eof1153;
case 1153:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 80: goto st1154;
		case 91: goto tr1359;
		case 112: goto st1154;
	}
	goto st1136;
st1154:
	if ( ++( p) == ( pe) )
		goto _test_eof1154;
case 1154:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 58: goto st1155;
		case 83: goto st1158;
		case 91: goto tr1359;
		case 115: goto st1158;
	}
	goto st1136;
st1155:
	if ( ++( p) == ( pe) )
		goto _test_eof1155;
case 1155:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 47: goto st1156;
		case 91: goto tr1359;
	}
	goto st1136;
st1156:
	if ( ++( p) == ( pe) )
		goto _test_eof1156;
case 1156:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 47: goto st1157;
		case 91: goto tr1359;
	}
	goto st1136;
st1157:
	if ( ++( p) == ( pe) )
		goto _test_eof1157;
case 1157:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 91: goto tr1371;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1136;
	goto st1145;
st1158:
	if ( ++( p) == ( pe) )
		goto _test_eof1158;
case 1158:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1358;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1358;
		case 58: goto st1155;
		case 91: goto tr1359;
	}
	goto st1136;
tr1351:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1159;
st1159:
	if ( ++( p) == ( pe) )
		goto _test_eof1159;
case 1159:
#line 25245 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 40: goto st1162;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
st1160:
	if ( ++( p) == ( pe) )
		goto _test_eof1160;
case 1160:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 32: goto tr254;
		case 34: goto tr1385;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st1160;
tr1385:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1161;
st1161:
	if ( ++( p) == ( pe) )
		goto _test_eof1161;
case 1161:
#line 25275 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1161;
		case 32: goto st1161;
		case 93: goto st1143;
	}
	goto tr254;
st1162:
	if ( ++( p) == ( pe) )
		goto _test_eof1162;
case 1162:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 35: goto tr1388;
		case 47: goto tr1388;
		case 72: goto tr1389;
		case 104: goto tr1389;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
tr1388:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1163;
st1163:
	if ( ++( p) == ( pe) )
		goto _test_eof1163;
case 1163:
#line 25306 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1391;
		case 41: goto tr1392;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1163;
tr1391:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1164;
st1164:
	if ( ++( p) == ( pe) )
		goto _test_eof1164;
case 1164:
#line 25324 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1161;
		case 32: goto st1161;
		case 41: goto tr994;
		case 93: goto st1165;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st812;
tr1451:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1165;
st1165:
	if ( ++( p) == ( pe) )
		goto _test_eof1165;
case 1165:
#line 25343 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1143;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1143;
		case 41: goto tr1395;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr1354;
	goto tr1394;
tr1392:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1928;
st1928:
	if ( ++( p) == ( pe) )
		goto _test_eof1928;
case 1928:
#line 25367 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2462;
		case 32: goto tr2462;
		case 34: goto tr1385;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2462;
	goto st1160;
tr1389:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1166;
st1166:
	if ( ++( p) == ( pe) )
		goto _test_eof1166;
case 1166:
#line 25384 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 84: goto st1167;
		case 116: goto st1167;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
st1167:
	if ( ++( p) == ( pe) )
		goto _test_eof1167;
case 1167:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 84: goto st1168;
		case 116: goto st1168;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
st1168:
	if ( ++( p) == ( pe) )
		goto _test_eof1168;
case 1168:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 80: goto st1169;
		case 112: goto st1169;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
st1169:
	if ( ++( p) == ( pe) )
		goto _test_eof1169;
case 1169:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 58: goto st1170;
		case 83: goto st1173;
		case 115: goto st1173;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
st1170:
	if ( ++( p) == ( pe) )
		goto _test_eof1170;
case 1170:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 47: goto st1171;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
st1171:
	if ( ++( p) == ( pe) )
		goto _test_eof1171;
case 1171:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 47: goto st1172;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
st1172:
	if ( ++( p) == ( pe) )
		goto _test_eof1172;
case 1172:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1391;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1163;
st1173:
	if ( ++( p) == ( pe) )
		goto _test_eof1173;
case 1173:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1385;
		case 58: goto st1170;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1160;
tr1348:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1174;
st1174:
	if ( ++( p) == ( pe) )
		goto _test_eof1174;
case 1174:
#line 25497 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1175;
		case 93: goto tr989;
		case 116: goto st1175;
	}
	goto st809;
st1175:
	if ( ++( p) == ( pe) )
		goto _test_eof1175;
case 1175:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1176;
		case 93: goto tr989;
		case 116: goto st1176;
	}
	goto st809;
st1176:
	if ( ++( p) == ( pe) )
		goto _test_eof1176;
case 1176:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st1177;
		case 93: goto tr989;
		case 112: goto st1177;
	}
	goto st809;
st1177:
	if ( ++( p) == ( pe) )
		goto _test_eof1177;
case 1177:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 58: goto st1178;
		case 83: goto st1181;
		case 93: goto tr989;
		case 115: goto st1181;
	}
	goto st809;
st1178:
	if ( ++( p) == ( pe) )
		goto _test_eof1178;
case 1178:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1179;
		case 93: goto tr989;
	}
	goto st809;
st1179:
	if ( ++( p) == ( pe) )
		goto _test_eof1179;
case 1179:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1180;
		case 93: goto tr989;
	}
	goto st809;
st1180:
	if ( ++( p) == ( pe) )
		goto _test_eof1180;
case 1180:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1351;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1133;
st1181:
	if ( ++( p) == ( pe) )
		goto _test_eof1181;
case 1181:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 58: goto st1178;
		case 93: goto tr989;
	}
	goto st809;
tr1344:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1182;
st1182:
	if ( ++( p) == ( pe) )
		goto _test_eof1182;
case 1182:
#line 25605 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1350;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1350;
		case 93: goto tr1411;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1182;
st1183:
	if ( ++( p) == ( pe) )
		goto _test_eof1183;
case 1183:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 35: goto tr1412;
		case 47: goto tr1412;
		case 72: goto tr1413;
		case 93: goto tr989;
		case 104: goto tr1413;
	}
	goto st809;
tr1412:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1184;
st1184:
	if ( ++( p) == ( pe) )
		goto _test_eof1184;
case 1184:
#line 25640 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 39: goto tr1350;
		case 93: goto tr1415;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1184;
tr1415:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1185;
st1185:
	if ( ++( p) == ( pe) )
		goto _test_eof1185;
case 1185:
#line 25660 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1385;
		case 40: goto st1187;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
st1186:
	if ( ++( p) == ( pe) )
		goto _test_eof1186;
case 1186:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 32: goto tr254;
		case 39: goto tr1385;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st1186;
st1187:
	if ( ++( p) == ( pe) )
		goto _test_eof1187;
case 1187:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 35: goto tr1418;
		case 39: goto tr1385;
		case 47: goto tr1418;
		case 72: goto tr1419;
		case 104: goto tr1419;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
tr1418:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1188;
st1188:
	if ( ++( p) == ( pe) )
		goto _test_eof1188;
case 1188:
#line 25706 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1391;
		case 41: goto tr1421;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1188;
tr1421:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1929;
st1929:
	if ( ++( p) == ( pe) )
		goto _test_eof1929;
case 1929:
#line 25728 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2462;
		case 32: goto tr2462;
		case 39: goto tr1385;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2462;
	goto st1186;
tr1419:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1189;
st1189:
	if ( ++( p) == ( pe) )
		goto _test_eof1189;
case 1189:
#line 25745 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1385;
		case 84: goto st1190;
		case 116: goto st1190;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
st1190:
	if ( ++( p) == ( pe) )
		goto _test_eof1190;
case 1190:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1385;
		case 84: goto st1191;
		case 116: goto st1191;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
st1191:
	if ( ++( p) == ( pe) )
		goto _test_eof1191;
case 1191:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1385;
		case 80: goto st1192;
		case 112: goto st1192;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
st1192:
	if ( ++( p) == ( pe) )
		goto _test_eof1192;
case 1192:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1385;
		case 58: goto st1193;
		case 83: goto st1196;
		case 115: goto st1196;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
st1193:
	if ( ++( p) == ( pe) )
		goto _test_eof1193;
case 1193:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1385;
		case 47: goto st1194;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
st1194:
	if ( ++( p) == ( pe) )
		goto _test_eof1194;
case 1194:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1385;
		case 47: goto st1195;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
st1195:
	if ( ++( p) == ( pe) )
		goto _test_eof1195;
case 1195:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1391;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1188;
st1196:
	if ( ++( p) == ( pe) )
		goto _test_eof1196;
case 1196:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 39: goto tr1385;
		case 58: goto st1193;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1186;
tr1413:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1197;
st1197:
	if ( ++( p) == ( pe) )
		goto _test_eof1197;
case 1197:
#line 25858 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1198;
		case 93: goto tr989;
		case 116: goto st1198;
	}
	goto st809;
st1198:
	if ( ++( p) == ( pe) )
		goto _test_eof1198;
case 1198:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1199;
		case 93: goto tr989;
		case 116: goto st1199;
	}
	goto st809;
st1199:
	if ( ++( p) == ( pe) )
		goto _test_eof1199;
case 1199:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st1200;
		case 93: goto tr989;
		case 112: goto st1200;
	}
	goto st809;
st1200:
	if ( ++( p) == ( pe) )
		goto _test_eof1200;
case 1200:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 58: goto st1201;
		case 83: goto st1204;
		case 93: goto tr989;
		case 115: goto st1204;
	}
	goto st809;
st1201:
	if ( ++( p) == ( pe) )
		goto _test_eof1201;
case 1201:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1202;
		case 93: goto tr989;
	}
	goto st809;
st1202:
	if ( ++( p) == ( pe) )
		goto _test_eof1202;
case 1202:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1203;
		case 93: goto tr989;
	}
	goto st809;
st1203:
	if ( ++( p) == ( pe) )
		goto _test_eof1203;
case 1203:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1415;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1184;
st1204:
	if ( ++( p) == ( pe) )
		goto _test_eof1204;
case 1204:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 58: goto st1201;
		case 93: goto tr989;
	}
	goto st809;
tr1346:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1205;
st1205:
	if ( ++( p) == ( pe) )
		goto _test_eof1205;
case 1205:
#line 25966 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1206;
		case 93: goto tr989;
		case 116: goto st1206;
	}
	goto st809;
st1206:
	if ( ++( p) == ( pe) )
		goto _test_eof1206;
case 1206:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 84: goto st1207;
		case 93: goto tr989;
		case 116: goto st1207;
	}
	goto st809;
st1207:
	if ( ++( p) == ( pe) )
		goto _test_eof1207;
case 1207:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 80: goto st1208;
		case 93: goto tr989;
		case 112: goto st1208;
	}
	goto st809;
st1208:
	if ( ++( p) == ( pe) )
		goto _test_eof1208;
case 1208:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 58: goto st1209;
		case 83: goto st1224;
		case 93: goto tr989;
		case 115: goto st1224;
	}
	goto st809;
st1209:
	if ( ++( p) == ( pe) )
		goto _test_eof1209;
case 1209:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1210;
		case 93: goto tr989;
	}
	goto st809;
st1210:
	if ( ++( p) == ( pe) )
		goto _test_eof1210;
case 1210:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1211;
		case 93: goto tr989;
	}
	goto st809;
st1211:
	if ( ++( p) == ( pe) )
		goto _test_eof1211;
case 1211:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st809;
		case 93: goto tr1443;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1182;
tr1443:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1212;
st1212:
	if ( ++( p) == ( pe) )
		goto _test_eof1212;
case 1212:
#line 26062 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 40: goto st1214;
		case 93: goto tr1446;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
st1213:
	if ( ++( p) == ( pe) )
		goto _test_eof1213;
case 1213:
	switch( (*( p)) ) {
		case 0: goto tr254;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 93: goto tr1446;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr254;
	goto st1213;
st1214:
	if ( ++( p) == ( pe) )
		goto _test_eof1214;
case 1214:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 35: goto tr1447;
		case 47: goto tr1447;
		case 72: goto tr1448;
		case 93: goto tr1446;
		case 104: goto tr1448;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
tr1447:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1215;
st1215:
	if ( ++( p) == ( pe) )
		goto _test_eof1215;
case 1215:
#line 26111 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 41: goto tr1450;
		case 93: goto tr1451;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1215;
tr1450:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1930;
st1930:
	if ( ++( p) == ( pe) )
		goto _test_eof1930;
case 1930:
#line 26134 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2462;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 93: goto tr1446;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr2462;
	goto st1213;
tr1448:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1216;
st1216:
	if ( ++( p) == ( pe) )
		goto _test_eof1216;
case 1216:
#line 26152 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 84: goto st1217;
		case 93: goto tr1446;
		case 116: goto st1217;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
st1217:
	if ( ++( p) == ( pe) )
		goto _test_eof1217;
case 1217:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 84: goto st1218;
		case 93: goto tr1446;
		case 116: goto st1218;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
st1218:
	if ( ++( p) == ( pe) )
		goto _test_eof1218;
case 1218:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 80: goto st1219;
		case 93: goto tr1446;
		case 112: goto st1219;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
st1219:
	if ( ++( p) == ( pe) )
		goto _test_eof1219;
case 1219:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 58: goto st1220;
		case 83: goto st1223;
		case 93: goto tr1446;
		case 115: goto st1223;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
st1220:
	if ( ++( p) == ( pe) )
		goto _test_eof1220;
case 1220:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 47: goto st1221;
		case 93: goto tr1446;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
st1221:
	if ( ++( p) == ( pe) )
		goto _test_eof1221;
case 1221:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 47: goto st1222;
		case 93: goto tr1446;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
st1222:
	if ( ++( p) == ( pe) )
		goto _test_eof1222;
case 1222:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 93: goto tr1451;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1215;
st1223:
	if ( ++( p) == ( pe) )
		goto _test_eof1223;
case 1223:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1385;
		case 32: goto tr1385;
		case 58: goto st1220;
		case 93: goto tr1446;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1213;
st1224:
	if ( ++( p) == ( pe) )
		goto _test_eof1224;
case 1224:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 58: goto st1209;
		case 93: goto tr989;
	}
	goto st809;
st1225:
	if ( ++( p) == ( pe) )
		goto _test_eof1225;
case 1225:
	switch( (*( p)) ) {
		case 9: goto st1225;
		case 32: goto st1225;
		case 35: goto tr1459;
		case 47: goto tr1459;
		case 72: goto tr1460;
		case 104: goto tr1460;
	}
	goto tr261;
tr1459:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1226;
st1226:
	if ( ++( p) == ( pe) )
		goto _test_eof1226;
case 1226:
#line 26298 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1462;
		case 32: goto tr1462;
		case 91: goto tr1463;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1226;
tr1462:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1227;
st1227:
	if ( ++( p) == ( pe) )
		goto _test_eof1227;
case 1227:
#line 26316 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1227;
		case 32: goto st1227;
		case 91: goto st1228;
	}
	goto tr261;
st1228:
	if ( ++( p) == ( pe) )
		goto _test_eof1228;
case 1228:
	if ( (*( p)) == 47 )
		goto st1229;
	goto tr261;
st1229:
	if ( ++( p) == ( pe) )
		goto _test_eof1229;
case 1229:
	switch( (*( p)) ) {
		case 85: goto st1230;
		case 117: goto st1230;
	}
	goto tr261;
st1230:
	if ( ++( p) == ( pe) )
		goto _test_eof1230;
case 1230:
	switch( (*( p)) ) {
		case 82: goto st1231;
		case 114: goto st1231;
	}
	goto tr261;
st1231:
	if ( ++( p) == ( pe) )
		goto _test_eof1231;
case 1231:
	switch( (*( p)) ) {
		case 76: goto st1232;
		case 108: goto st1232;
	}
	goto tr261;
st1232:
	if ( ++( p) == ( pe) )
		goto _test_eof1232;
case 1232:
	if ( (*( p)) == 93 )
		goto tr1470;
	goto tr261;
tr1463:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1233;
st1233:
	if ( ++( p) == ( pe) )
		goto _test_eof1233;
case 1233:
#line 26372 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1462;
		case 32: goto tr1462;
		case 47: goto st1234;
		case 91: goto tr1463;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1226;
st1234:
	if ( ++( p) == ( pe) )
		goto _test_eof1234;
case 1234:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1462;
		case 32: goto tr1462;
		case 85: goto st1235;
		case 91: goto tr1463;
		case 117: goto st1235;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1226;
st1235:
	if ( ++( p) == ( pe) )
		goto _test_eof1235;
case 1235:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1462;
		case 32: goto tr1462;
		case 82: goto st1236;
		case 91: goto tr1463;
		case 114: goto st1236;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1226;
st1236:
	if ( ++( p) == ( pe) )
		goto _test_eof1236;
case 1236:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1462;
		case 32: goto tr1462;
		case 76: goto st1237;
		case 91: goto tr1463;
		case 108: goto st1237;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1226;
st1237:
	if ( ++( p) == ( pe) )
		goto _test_eof1237;
case 1237:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1462;
		case 32: goto tr1462;
		case 91: goto tr1463;
		case 93: goto tr1470;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1226;
tr1460:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1238;
st1238:
	if ( ++( p) == ( pe) )
		goto _test_eof1238;
case 1238:
#line 26450 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1239;
		case 116: goto st1239;
	}
	goto tr261;
st1239:
	if ( ++( p) == ( pe) )
		goto _test_eof1239;
case 1239:
	switch( (*( p)) ) {
		case 84: goto st1240;
		case 116: goto st1240;
	}
	goto tr261;
st1240:
	if ( ++( p) == ( pe) )
		goto _test_eof1240;
case 1240:
	switch( (*( p)) ) {
		case 80: goto st1241;
		case 112: goto st1241;
	}
	goto tr261;
st1241:
	if ( ++( p) == ( pe) )
		goto _test_eof1241;
case 1241:
	switch( (*( p)) ) {
		case 58: goto st1242;
		case 83: goto st1245;
		case 115: goto st1245;
	}
	goto tr261;
st1242:
	if ( ++( p) == ( pe) )
		goto _test_eof1242;
case 1242:
	if ( (*( p)) == 47 )
		goto st1243;
	goto tr261;
st1243:
	if ( ++( p) == ( pe) )
		goto _test_eof1243;
case 1243:
	if ( (*( p)) == 47 )
		goto st1244;
	goto tr261;
st1244:
	if ( ++( p) == ( pe) )
		goto _test_eof1244;
case 1244:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1226;
st1245:
	if ( ++( p) == ( pe) )
		goto _test_eof1245;
case 1245:
	if ( (*( p)) == 58 )
		goto st1242;
	goto tr261;
tr2459:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1246;
st1246:
	if ( ++( p) == ( pe) )
		goto _test_eof1246;
case 1246:
#line 26524 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr606;
		case 9: goto tr1483;
		case 10: goto tr608;
		case 13: goto tr608;
		case 32: goto tr1483;
		case 35: goto tr1485;
		case 58: goto tr1486;
		case 60: goto tr1487;
		case 62: goto tr1488;
		case 92: goto tr1489;
		case 93: goto tr989;
		case 124: goto tr1490;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr1484;
	goto tr1482;
tr1482:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1247;
st1247:
	if ( ++( p) == ( pe) )
		goto _test_eof1247;
case 1247:
#line 26550 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto tr1492;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto tr1492;
		case 35: goto tr1494;
		case 93: goto tr1495;
		case 124: goto tr1496;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1249;
	goto st1247;
tr1492:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1248;
st1248:
	if ( ++( p) == ( pe) )
		goto _test_eof1248;
case 1248:
#line 26572 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto st1248;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto st1248;
		case 35: goto st1250;
		case 93: goto tr1499;
		case 124: goto st1254;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1249;
	goto st1247;
tr1484:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1249;
st1249:
	if ( ++( p) == ( pe) )
		goto _test_eof1249;
case 1249:
#line 26594 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto st1249;
		case 93: goto tr989;
		case 124: goto st809;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st1249;
	goto st1247;
tr1494:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1250;
st1250:
	if ( ++( p) == ( pe) )
		goto _test_eof1250;
case 1250:
#line 26614 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto tr1492;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto tr1492;
		case 35: goto tr1494;
		case 93: goto tr1495;
		case 124: goto tr1496;
	}
	if ( (*( p)) > 12 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr1501;
	} else if ( (*( p)) >= 11 )
		goto st1249;
	goto st1247;
tr1501:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1251;
st1251:
	if ( ++( p) == ( pe) )
		goto _test_eof1251;
case 1251:
#line 26639 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1502;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1503;
		case 45: goto st1259;
		case 93: goto tr1506;
		case 95: goto st1259;
		case 124: goto tr1507;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1251;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1251;
	} else
		goto st1251;
	goto st809;
tr1502:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1252;
st1252:
	if ( ++( p) == ( pe) )
		goto _test_eof1252;
case 1252:
#line 26668 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1252;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1252;
		case 93: goto tr1499;
		case 124: goto st1254;
	}
	goto st809;
tr1499:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1253;
tr1495:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1253;
tr1506:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1253;
st1253:
	if ( ++( p) == ( pe) )
		goto _test_eof1253;
case 1253:
#line 26699 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st811;
		case 93: goto st1731;
	}
	goto tr261;
tr1496:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1254;
tr1507:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1254;
tr1510:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1254;
st1254:
	if ( ++( p) == ( pe) )
		goto _test_eof1254;
case 1254:
#line 26723 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr634;
		case 9: goto tr1510;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1510;
		case 93: goto tr1511;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto tr1509;
tr1509:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1255;
st1255:
	if ( ++( p) == ( pe) )
		goto _test_eof1255;
case 1255:
#line 26744 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto tr1513;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1513;
		case 93: goto tr1514;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1255;
tr1513:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1256;
st1256:
	if ( ++( p) == ( pe) )
		goto _test_eof1256;
case 1256:
#line 26765 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto st1256;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1256;
		case 93: goto tr1516;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1255;
tr1516:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1257;
tr1511:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1257;
tr1514:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1257;
st1257:
	if ( ++( p) == ( pe) )
		goto _test_eof1257;
case 1257:
#line 26800 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st811;
		case 93: goto st1733;
	}
	goto tr261;
tr1503:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1258;
st1258:
	if ( ++( p) == ( pe) )
		goto _test_eof1258;
case 1258:
#line 26814 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1252;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1258;
		case 45: goto st1259;
		case 93: goto tr1499;
		case 95: goto st1259;
		case 124: goto st1254;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1251;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1251;
	} else
		goto st1251;
	goto st809;
st1259:
	if ( ++( p) == ( pe) )
		goto _test_eof1259;
case 1259:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1259;
		case 45: goto st1259;
		case 93: goto tr989;
		case 95: goto st1259;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1251;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1251;
	} else
		goto st1251;
	goto st809;
tr1483:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1260;
st1260:
	if ( ++( p) == ( pe) )
		goto _test_eof1260;
case 1260:
#line 26865 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr606;
		case 9: goto tr1483;
		case 10: goto tr608;
		case 13: goto tr608;
		case 32: goto tr1483;
		case 58: goto tr1486;
		case 60: goto tr1487;
		case 62: goto tr1488;
		case 92: goto tr1489;
		case 93: goto tr989;
		case 124: goto tr1490;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr1484;
	goto tr1482;
tr1486:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1261;
st1261:
	if ( ++( p) == ( pe) )
		goto _test_eof1261;
case 1261:
#line 26890 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto tr1492;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto tr1492;
		case 35: goto tr1494;
		case 93: goto tr1495;
		case 124: goto tr1518;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1249;
	goto st1247;
tr1518:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1262;
st1262:
	if ( ++( p) == ( pe) )
		goto _test_eof1262;
case 1262:
#line 26912 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr634;
		case 9: goto tr1519;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1519;
		case 35: goto tr1520;
		case 93: goto tr1521;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto tr1509;
tr1522:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1263;
tr1519:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1263;
st1263:
	if ( ++( p) == ( pe) )
		goto _test_eof1263;
case 1263:
#line 26944 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr634;
		case 9: goto tr1522;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1522;
		case 35: goto tr1523;
		case 93: goto tr1524;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto tr1509;
tr1557:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1264;
tr1523:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1264;
tr1520:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1264;
st1264:
	if ( ++( p) == ( pe) )
		goto _test_eof1264;
case 1264:
#line 26976 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto tr1513;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1513;
		case 93: goto tr1514;
		case 124: goto st809;
	}
	if ( (*( p)) > 12 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr1525;
	} else if ( (*( p)) >= 11 )
		goto st809;
	goto st1255;
tr1525:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1265;
st1265:
	if ( ++( p) == ( pe) )
		goto _test_eof1265;
case 1265:
#line 27000 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto tr1526;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1527;
		case 45: goto st1269;
		case 93: goto tr1530;
		case 95: goto st1269;
		case 124: goto st809;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st809;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1265;
		} else if ( (*( p)) >= 65 )
			goto st1265;
	} else
		goto st1265;
	goto st1255;
tr1526:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1266;
st1266:
	if ( ++( p) == ( pe) )
		goto _test_eof1266;
case 1266:
#line 27034 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto st1266;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1266;
		case 93: goto tr1532;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1255;
tr1532:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1267;
tr1524:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1267;
tr1521:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1267;
tr1530:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1267;
tr1558:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1267;
st1267:
	if ( ++( p) == ( pe) )
		goto _test_eof1267;
case 1267:
#line 27089 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st811;
		case 93: goto st1735;
	}
	goto tr261;
tr1527:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1268;
st1268:
	if ( ++( p) == ( pe) )
		goto _test_eof1268;
case 1268:
#line 27105 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto st1266;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1268;
		case 45: goto st1269;
		case 93: goto tr1532;
		case 95: goto st1269;
		case 124: goto st809;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st809;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1265;
		} else if ( (*( p)) >= 65 )
			goto st1265;
	} else
		goto st1265;
	goto st1255;
st1269:
	if ( ++( p) == ( pe) )
		goto _test_eof1269;
case 1269:
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto tr1513;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1534;
		case 45: goto st1269;
		case 93: goto tr1514;
		case 95: goto st1269;
		case 124: goto st809;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st809;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1265;
		} else if ( (*( p)) >= 65 )
			goto st1265;
	} else
		goto st1265;
	goto st1255;
tr1534:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1270;
st1270:
	if ( ++( p) == ( pe) )
		goto _test_eof1270;
case 1270:
#line 27164 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto st1256;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1270;
		case 45: goto st1269;
		case 93: goto tr1516;
		case 95: goto st1269;
		case 124: goto st809;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st809;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1265;
		} else if ( (*( p)) >= 65 )
			goto st1265;
	} else
		goto st1265;
	goto st1255;
tr1487:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1271;
st1271:
	if ( ++( p) == ( pe) )
		goto _test_eof1271;
case 1271:
#line 27196 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto tr1492;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto tr1492;
		case 35: goto tr1494;
		case 93: goto tr1495;
		case 124: goto tr1536;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1249;
	goto st1247;
tr1536:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1272;
st1272:
	if ( ++( p) == ( pe) )
		goto _test_eof1272;
case 1272:
#line 27218 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr634;
		case 9: goto tr1510;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1510;
		case 62: goto tr1537;
		case 93: goto tr1511;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto tr1509;
tr1537:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1273;
st1273:
	if ( ++( p) == ( pe) )
		goto _test_eof1273;
case 1273:
#line 27240 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto tr1513;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1513;
		case 93: goto tr1514;
		case 95: goto st1274;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1255;
st1274:
	if ( ++( p) == ( pe) )
		goto _test_eof1274;
case 1274:
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto tr1513;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1513;
		case 60: goto st1275;
		case 93: goto tr1514;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1255;
st1275:
	if ( ++( p) == ( pe) )
		goto _test_eof1275;
case 1275:
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto tr1513;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1513;
		case 93: goto tr1514;
		case 124: goto st1276;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1255;
st1276:
	if ( ++( p) == ( pe) )
		goto _test_eof1276;
case 1276:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 62: goto st1277;
		case 93: goto tr989;
	}
	goto st809;
st1277:
	if ( ++( p) == ( pe) )
		goto _test_eof1277;
case 1277:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1542;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1542;
		case 35: goto tr1543;
		case 93: goto tr1495;
	}
	goto st809;
tr1542:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1278;
st1278:
	if ( ++( p) == ( pe) )
		goto _test_eof1278;
case 1278:
#line 27321 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1278;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1278;
		case 35: goto st1279;
		case 93: goto tr1499;
	}
	goto st809;
tr1543:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1279;
st1279:
	if ( ++( p) == ( pe) )
		goto _test_eof1279;
case 1279:
#line 27340 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
	}
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr1546;
	goto st809;
tr1546:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1280;
st1280:
	if ( ++( p) == ( pe) )
		goto _test_eof1280;
case 1280:
#line 27358 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1547;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1548;
		case 45: goto st1283;
		case 93: goto tr1506;
		case 95: goto st1283;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1280;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1280;
	} else
		goto st1280;
	goto st809;
tr1547:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1281;
st1281:
	if ( ++( p) == ( pe) )
		goto _test_eof1281;
case 1281:
#line 27386 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1281;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1281;
		case 93: goto tr1499;
	}
	goto st809;
tr1548:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1282;
st1282:
	if ( ++( p) == ( pe) )
		goto _test_eof1282;
case 1282:
#line 27404 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1281;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1282;
		case 45: goto st1283;
		case 93: goto tr1499;
		case 95: goto st1283;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1280;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1280;
	} else
		goto st1280;
	goto st809;
st1283:
	if ( ++( p) == ( pe) )
		goto _test_eof1283;
case 1283:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1283;
		case 45: goto st1283;
		case 93: goto tr989;
		case 95: goto st1283;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1280;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1280;
	} else
		goto st1280;
	goto st809;
tr1488:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1284;
st1284:
	if ( ++( p) == ( pe) )
		goto _test_eof1284;
case 1284:
#line 27454 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto tr1492;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto tr1492;
		case 35: goto tr1494;
		case 58: goto st1261;
		case 93: goto tr1495;
		case 124: goto tr1554;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1249;
	goto st1247;
tr1554:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1285;
st1285:
	if ( ++( p) == ( pe) )
		goto _test_eof1285;
case 1285:
#line 27477 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr634;
		case 9: goto tr1510;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1510;
		case 51: goto tr1555;
		case 93: goto tr1511;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto tr1509;
tr1555:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1286;
st1286:
	if ( ++( p) == ( pe) )
		goto _test_eof1286;
case 1286:
#line 27499 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto tr1556;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1556;
		case 35: goto tr1557;
		case 93: goto tr1558;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1255;
tr1556:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1287;
st1287:
	if ( ++( p) == ( pe) )
		goto _test_eof1287;
case 1287:
#line 27523 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st529;
		case 9: goto st1287;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1287;
		case 35: goto st1264;
		case 93: goto tr1532;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1255;
tr1489:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1288;
st1288:
	if ( ++( p) == ( pe) )
		goto _test_eof1288;
case 1288:
#line 27545 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto tr1492;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto tr1492;
		case 35: goto tr1494;
		case 93: goto tr1495;
		case 124: goto tr1561;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1249;
	goto st1247;
tr1561:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1289;
st1289:
	if ( ++( p) == ( pe) )
		goto _test_eof1289;
case 1289:
#line 27567 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr634;
		case 9: goto tr1510;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1510;
		case 93: goto tr1511;
		case 124: goto st1290;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto tr1509;
st1290:
	if ( ++( p) == ( pe) )
		goto _test_eof1290;
case 1290:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1277;
		case 93: goto tr989;
	}
	goto st809;
tr1490:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1291;
st1291:
	if ( ++( p) == ( pe) )
		goto _test_eof1291;
case 1291:
#line 27600 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
		case 95: goto st1295;
		case 119: goto st1296;
		case 124: goto st1297;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1292;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1292;
	} else
		goto st1292;
	goto st809;
st1292:
	if ( ++( p) == ( pe) )
		goto _test_eof1292;
case 1292:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1567;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1567;
		case 35: goto tr1568;
		case 93: goto tr1495;
		case 124: goto tr1496;
	}
	goto st809;
tr1567:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1293;
st1293:
	if ( ++( p) == ( pe) )
		goto _test_eof1293;
case 1293:
#line 27642 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto st1293;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1293;
		case 35: goto st1294;
		case 93: goto tr1499;
		case 124: goto st1254;
	}
	goto st809;
tr1568:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1294;
st1294:
	if ( ++( p) == ( pe) )
		goto _test_eof1294;
case 1294:
#line 27662 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
	}
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr1501;
	goto st809;
st1295:
	if ( ++( p) == ( pe) )
		goto _test_eof1295;
case 1295:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
		case 124: goto st1292;
	}
	goto st809;
st1296:
	if ( ++( p) == ( pe) )
		goto _test_eof1296;
case 1296:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 9: goto tr1567;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1567;
		case 35: goto tr1568;
		case 93: goto tr1495;
		case 124: goto tr1518;
	}
	goto st809;
st1297:
	if ( ++( p) == ( pe) )
		goto _test_eof1297;
case 1297:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
		case 95: goto st1298;
	}
	goto st809;
st1298:
	if ( ++( p) == ( pe) )
		goto _test_eof1298;
case 1298:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 93: goto tr989;
		case 124: goto st1295;
	}
	goto st809;
tr1485:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1299;
st1299:
	if ( ++( p) == ( pe) )
		goto _test_eof1299;
case 1299:
#line 27731 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto tr1492;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto tr1492;
		case 35: goto tr1494;
		case 45: goto tr1572;
		case 93: goto tr1495;
		case 95: goto tr1572;
		case 124: goto tr1496;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st1249;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr1572;
		} else if ( (*( p)) >= 65 )
			goto tr1572;
	} else
		goto tr1572;
	goto st1247;
tr1572:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1300;
st1300:
	if ( ++( p) == ( pe) )
		goto _test_eof1300;
case 1300:
#line 27764 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st521;
		case 9: goto tr1492;
		case 10: goto st523;
		case 13: goto st523;
		case 32: goto tr1492;
		case 35: goto tr1494;
		case 45: goto st1300;
		case 93: goto tr1574;
		case 95: goto st1300;
		case 124: goto tr1575;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st1249;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1300;
		} else if ( (*( p)) >= 65 )
			goto st1300;
	} else
		goto st1300;
	goto st1247;
tr1574:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1301;
st1301:
	if ( ++( p) == ( pe) )
		goto _test_eof1301;
case 1301:
#line 27799 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st811;
		case 93: goto st1737;
	}
	goto tr261;
tr1575:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1302;
tr1577:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1302;
st1302:
	if ( ++( p) == ( pe) )
		goto _test_eof1302;
case 1302:
#line 27819 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr704;
		case 9: goto tr1577;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1577;
		case 93: goto tr1578;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto tr1576;
tr1576:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1303;
st1303:
	if ( ++( p) == ( pe) )
		goto _test_eof1303;
case 1303:
#line 27840 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st577;
		case 9: goto tr1580;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto tr1580;
		case 93: goto tr1581;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1303;
tr1580:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1304;
st1304:
	if ( ++( p) == ( pe) )
		goto _test_eof1304;
case 1304:
#line 27861 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st577;
		case 9: goto st1304;
		case 10: goto tr261;
		case 13: goto tr261;
		case 32: goto st1304;
		case 93: goto tr1583;
		case 124: goto st809;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st809;
	goto st1303;
tr1583:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1305;
tr1578:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1305;
tr1581:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1305;
st1305:
	if ( ++( p) == ( pe) )
		goto _test_eof1305;
case 1305:
#line 27896 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st811;
		case 93: goto st1739;
	}
	goto tr261;
tr2173:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1931;
st1931:
	if ( ++( p) == ( pe) )
		goto _test_eof1931;
case 1931:
#line 27916 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 123 )
		goto st581;
	goto tr2177;
tr2174:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1932;
st1932:
	if ( ++( p) == ( pe) )
		goto _test_eof1932;
case 1932:
#line 27932 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1306;
		case 65: goto st1320;
		case 66: goto st1343;
		case 67: goto st1345;
		case 69: goto st1352;
		case 72: goto tr2469;
		case 73: goto st1353;
		case 78: goto st1363;
		case 81: goto st254;
		case 83: goto st1370;
		case 84: goto st1386;
		case 85: goto st1388;
		case 97: goto st1320;
		case 98: goto st1343;
		case 99: goto st1345;
		case 101: goto st1352;
		case 104: goto tr2469;
		case 105: goto st1353;
		case 110: goto st1363;
		case 113: goto st254;
		case 115: goto st1370;
		case 116: goto st1386;
		case 117: goto st1388;
	}
	goto tr2177;
st1306:
	if ( ++( p) == ( pe) )
		goto _test_eof1306;
case 1306:
	switch( (*( p)) ) {
		case 66: goto st1307;
		case 69: goto st1308;
		case 73: goto st1309;
		case 81: goto st234;
		case 83: goto st1310;
		case 84: goto st318;
		case 85: goto st1319;
		case 98: goto st1307;
		case 101: goto st1308;
		case 105: goto st1309;
		case 113: goto st234;
		case 115: goto st1310;
		case 116: goto st318;
		case 117: goto st1319;
	}
	goto tr261;
st1307:
	if ( ++( p) == ( pe) )
		goto _test_eof1307;
case 1307:
	switch( (*( p)) ) {
		case 62: goto tr1033;
		case 76: goto st225;
		case 108: goto st225;
	}
	goto tr261;
st1308:
	if ( ++( p) == ( pe) )
		goto _test_eof1308;
case 1308:
	switch( (*( p)) ) {
		case 77: goto st1309;
		case 109: goto st1309;
	}
	goto tr261;
st1309:
	if ( ++( p) == ( pe) )
		goto _test_eof1309;
case 1309:
	if ( (*( p)) == 62 )
		goto tr1044;
	goto tr261;
st1310:
	if ( ++( p) == ( pe) )
		goto _test_eof1310;
case 1310:
	switch( (*( p)) ) {
		case 62: goto tr1057;
		case 69: goto st240;
		case 80: goto st311;
		case 84: goto st1311;
		case 85: goto st1316;
		case 101: goto st240;
		case 112: goto st311;
		case 116: goto st1311;
		case 117: goto st1316;
	}
	goto tr261;
st1311:
	if ( ++( p) == ( pe) )
		goto _test_eof1311;
case 1311:
	switch( (*( p)) ) {
		case 82: goto st1312;
		case 114: goto st1312;
	}
	goto tr261;
st1312:
	if ( ++( p) == ( pe) )
		goto _test_eof1312;
case 1312:
	switch( (*( p)) ) {
		case 79: goto st1313;
		case 111: goto st1313;
	}
	goto tr261;
st1313:
	if ( ++( p) == ( pe) )
		goto _test_eof1313;
case 1313:
	switch( (*( p)) ) {
		case 78: goto st1314;
		case 110: goto st1314;
	}
	goto tr261;
st1314:
	if ( ++( p) == ( pe) )
		goto _test_eof1314;
case 1314:
	switch( (*( p)) ) {
		case 71: goto st1315;
		case 103: goto st1315;
	}
	goto tr261;
st1315:
	if ( ++( p) == ( pe) )
		goto _test_eof1315;
case 1315:
	if ( (*( p)) == 62 )
		goto tr1033;
	goto tr261;
st1316:
	if ( ++( p) == ( pe) )
		goto _test_eof1316;
case 1316:
	switch( (*( p)) ) {
		case 66: goto st1317;
		case 80: goto st1318;
		case 98: goto st1317;
		case 112: goto st1318;
	}
	goto tr261;
st1317:
	if ( ++( p) == ( pe) )
		goto _test_eof1317;
case 1317:
	if ( (*( p)) == 62 )
		goto tr1597;
	goto tr261;
st1318:
	if ( ++( p) == ( pe) )
		goto _test_eof1318;
case 1318:
	if ( (*( p)) == 62 )
		goto tr1598;
	goto tr261;
st1319:
	if ( ++( p) == ( pe) )
		goto _test_eof1319;
case 1319:
	if ( (*( p)) == 62 )
		goto tr1084;
	goto tr261;
st1320:
	if ( ++( p) == ( pe) )
		goto _test_eof1320;
case 1320:
	switch( (*( p)) ) {
		case 9: goto st1321;
		case 32: goto st1321;
	}
	goto tr261;
st1321:
	if ( ++( p) == ( pe) )
		goto _test_eof1321;
case 1321:
	switch( (*( p)) ) {
		case 9: goto st1321;
		case 32: goto st1321;
		case 72: goto st1322;
		case 104: goto st1322;
	}
	goto tr261;
st1322:
	if ( ++( p) == ( pe) )
		goto _test_eof1322;
case 1322:
	switch( (*( p)) ) {
		case 82: goto st1323;
		case 114: goto st1323;
	}
	goto tr261;
st1323:
	if ( ++( p) == ( pe) )
		goto _test_eof1323;
case 1323:
	switch( (*( p)) ) {
		case 69: goto st1324;
		case 101: goto st1324;
	}
	goto tr261;
st1324:
	if ( ++( p) == ( pe) )
		goto _test_eof1324;
case 1324:
	switch( (*( p)) ) {
		case 70: goto st1325;
		case 102: goto st1325;
	}
	goto tr261;
st1325:
	if ( ++( p) == ( pe) )
		goto _test_eof1325;
case 1325:
	if ( (*( p)) == 61 )
		goto st1326;
	goto tr261;
st1326:
	if ( ++( p) == ( pe) )
		goto _test_eof1326;
case 1326:
	if ( (*( p)) == 34 )
		goto st1327;
	goto tr261;
st1327:
	if ( ++( p) == ( pe) )
		goto _test_eof1327;
case 1327:
	switch( (*( p)) ) {
		case 35: goto tr1606;
		case 47: goto tr1606;
		case 72: goto tr1607;
		case 104: goto tr1607;
	}
	goto tr261;
tr1606:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1328;
st1328:
	if ( ++( p) == ( pe) )
		goto _test_eof1328;
case 1328:
#line 28177 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1609;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1328;
tr1609:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1329;
st1329:
	if ( ++( p) == ( pe) )
		goto _test_eof1329;
case 1329:
#line 28194 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 34: goto tr1609;
		case 62: goto st1330;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1328;
st1330:
	if ( ++( p) == ( pe) )
		goto _test_eof1330;
case 1330:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
	}
	goto tr1611;
tr1611:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1331;
st1331:
	if ( ++( p) == ( pe) )
		goto _test_eof1331;
case 1331:
#line 28222 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 60: goto tr1613;
	}
	goto st1331;
tr1613:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1332;
st1332:
	if ( ++( p) == ( pe) )
		goto _test_eof1332;
case 1332:
#line 28238 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 47: goto st1333;
		case 60: goto tr1613;
	}
	goto st1331;
st1333:
	if ( ++( p) == ( pe) )
		goto _test_eof1333;
case 1333:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 60: goto tr1613;
		case 65: goto st1334;
		case 97: goto st1334;
	}
	goto st1331;
st1334:
	if ( ++( p) == ( pe) )
		goto _test_eof1334;
case 1334:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 10: goto tr261;
		case 13: goto tr261;
		case 60: goto tr1613;
		case 62: goto tr1616;
	}
	goto st1331;
tr1607:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1335;
st1335:
	if ( ++( p) == ( pe) )
		goto _test_eof1335;
case 1335:
#line 28280 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1336;
		case 116: goto st1336;
	}
	goto tr261;
st1336:
	if ( ++( p) == ( pe) )
		goto _test_eof1336;
case 1336:
	switch( (*( p)) ) {
		case 84: goto st1337;
		case 116: goto st1337;
	}
	goto tr261;
st1337:
	if ( ++( p) == ( pe) )
		goto _test_eof1337;
case 1337:
	switch( (*( p)) ) {
		case 80: goto st1338;
		case 112: goto st1338;
	}
	goto tr261;
st1338:
	if ( ++( p) == ( pe) )
		goto _test_eof1338;
case 1338:
	switch( (*( p)) ) {
		case 58: goto st1339;
		case 83: goto st1342;
		case 115: goto st1342;
	}
	goto tr261;
st1339:
	if ( ++( p) == ( pe) )
		goto _test_eof1339;
case 1339:
	if ( (*( p)) == 47 )
		goto st1340;
	goto tr261;
st1340:
	if ( ++( p) == ( pe) )
		goto _test_eof1340;
case 1340:
	if ( (*( p)) == 47 )
		goto st1341;
	goto tr261;
st1341:
	if ( ++( p) == ( pe) )
		goto _test_eof1341;
case 1341:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1328;
st1342:
	if ( ++( p) == ( pe) )
		goto _test_eof1342;
case 1342:
	if ( (*( p)) == 58 )
		goto st1339;
	goto tr261;
st1343:
	if ( ++( p) == ( pe) )
		goto _test_eof1343;
case 1343:
	switch( (*( p)) ) {
		case 62: goto tr1086;
		case 76: goto st250;
		case 82: goto st1344;
		case 108: goto st250;
		case 114: goto st1344;
	}
	goto tr261;
st1344:
	if ( ++( p) == ( pe) )
		goto _test_eof1344;
case 1344:
	if ( (*( p)) == 62 )
		goto tr1087;
	goto tr261;
st1345:
	if ( ++( p) == ( pe) )
		goto _test_eof1345;
case 1345:
	switch( (*( p)) ) {
		case 79: goto st1346;
		case 111: goto st1346;
	}
	goto tr261;
st1346:
	if ( ++( p) == ( pe) )
		goto _test_eof1346;
case 1346:
	switch( (*( p)) ) {
		case 68: goto st1347;
		case 100: goto st1347;
	}
	goto tr261;
st1347:
	if ( ++( p) == ( pe) )
		goto _test_eof1347;
case 1347:
	switch( (*( p)) ) {
		case 69: goto st1348;
		case 101: goto st1348;
	}
	goto tr261;
st1348:
	if ( ++( p) == ( pe) )
		goto _test_eof1348;
case 1348:
	switch( (*( p)) ) {
		case 9: goto st1349;
		case 32: goto st1349;
		case 61: goto st1350;
		case 62: goto tr1094;
	}
	goto tr261;
st1349:
	if ( ++( p) == ( pe) )
		goto _test_eof1349;
case 1349:
	switch( (*( p)) ) {
		case 9: goto st1349;
		case 32: goto st1349;
		case 61: goto st1350;
	}
	goto tr261;
st1350:
	if ( ++( p) == ( pe) )
		goto _test_eof1350;
case 1350:
	switch( (*( p)) ) {
		case 9: goto st1350;
		case 32: goto st1350;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1630;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1630;
	} else
		goto tr1630;
	goto tr261;
tr1630:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1351;
st1351:
	if ( ++( p) == ( pe) )
		goto _test_eof1351;
case 1351:
#line 28438 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 62 )
		goto tr1632;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1351;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1351;
	} else
		goto st1351;
	goto tr261;
tr1632:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1933;
st1933:
	if ( ++( p) == ( pe) )
		goto _test_eof1933;
case 1933:
#line 28460 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1099;
		case 9: goto st901;
		case 10: goto tr1099;
		case 32: goto st901;
	}
	goto tr2460;
st1352:
	if ( ++( p) == ( pe) )
		goto _test_eof1352;
case 1352:
	switch( (*( p)) ) {
		case 77: goto st1353;
		case 109: goto st1353;
	}
	goto tr261;
st1353:
	if ( ++( p) == ( pe) )
		goto _test_eof1353;
case 1353:
	if ( (*( p)) == 62 )
		goto tr1292;
	goto tr261;
tr2469:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1354;
st1354:
	if ( ++( p) == ( pe) )
		goto _test_eof1354;
case 1354:
#line 28492 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1355;
		case 116: goto st1355;
	}
	goto tr261;
st1355:
	if ( ++( p) == ( pe) )
		goto _test_eof1355;
case 1355:
	switch( (*( p)) ) {
		case 84: goto st1356;
		case 116: goto st1356;
	}
	goto tr261;
st1356:
	if ( ++( p) == ( pe) )
		goto _test_eof1356;
case 1356:
	switch( (*( p)) ) {
		case 80: goto st1357;
		case 112: goto st1357;
	}
	goto tr261;
st1357:
	if ( ++( p) == ( pe) )
		goto _test_eof1357;
case 1357:
	switch( (*( p)) ) {
		case 58: goto st1358;
		case 83: goto st1362;
		case 115: goto st1362;
	}
	goto tr261;
st1358:
	if ( ++( p) == ( pe) )
		goto _test_eof1358;
case 1358:
	if ( (*( p)) == 47 )
		goto st1359;
	goto tr261;
st1359:
	if ( ++( p) == ( pe) )
		goto _test_eof1359;
case 1359:
	if ( (*( p)) == 47 )
		goto st1360;
	goto tr261;
st1360:
	if ( ++( p) == ( pe) )
		goto _test_eof1360;
case 1360:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1361;
st1361:
	if ( ++( p) == ( pe) )
		goto _test_eof1361;
case 1361:
	switch( (*( p)) ) {
		case 0: goto tr261;
		case 32: goto tr261;
		case 62: goto tr1642;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr261;
	goto st1361;
st1362:
	if ( ++( p) == ( pe) )
		goto _test_eof1362;
case 1362:
	if ( (*( p)) == 58 )
		goto st1358;
	goto tr261;
st1363:
	if ( ++( p) == ( pe) )
		goto _test_eof1363;
case 1363:
	switch( (*( p)) ) {
		case 79: goto st1364;
		case 111: goto st1364;
	}
	goto tr261;
st1364:
	if ( ++( p) == ( pe) )
		goto _test_eof1364;
case 1364:
	switch( (*( p)) ) {
		case 68: goto st1365;
		case 100: goto st1365;
	}
	goto tr261;
st1365:
	if ( ++( p) == ( pe) )
		goto _test_eof1365;
case 1365:
	switch( (*( p)) ) {
		case 84: goto st1366;
		case 116: goto st1366;
	}
	goto tr261;
st1366:
	if ( ++( p) == ( pe) )
		goto _test_eof1366;
case 1366:
	switch( (*( p)) ) {
		case 69: goto st1367;
		case 101: goto st1367;
	}
	goto tr261;
st1367:
	if ( ++( p) == ( pe) )
		goto _test_eof1367;
case 1367:
	switch( (*( p)) ) {
		case 88: goto st1368;
		case 120: goto st1368;
	}
	goto tr261;
st1368:
	if ( ++( p) == ( pe) )
		goto _test_eof1368;
case 1368:
	switch( (*( p)) ) {
		case 84: goto st1369;
		case 116: goto st1369;
	}
	goto tr261;
st1369:
	if ( ++( p) == ( pe) )
		goto _test_eof1369;
case 1369:
	if ( (*( p)) == 62 )
		goto tr1299;
	goto tr261;
st1370:
	if ( ++( p) == ( pe) )
		goto _test_eof1370;
case 1370:
	switch( (*( p)) ) {
		case 62: goto tr1310;
		case 80: goto st1371;
		case 84: goto st1378;
		case 85: goto st1383;
		case 112: goto st1371;
		case 116: goto st1378;
		case 117: goto st1383;
	}
	goto tr261;
st1371:
	if ( ++( p) == ( pe) )
		goto _test_eof1371;
case 1371:
	switch( (*( p)) ) {
		case 79: goto st1372;
		case 111: goto st1372;
	}
	goto tr261;
st1372:
	if ( ++( p) == ( pe) )
		goto _test_eof1372;
case 1372:
	switch( (*( p)) ) {
		case 73: goto st1373;
		case 105: goto st1373;
	}
	goto tr261;
st1373:
	if ( ++( p) == ( pe) )
		goto _test_eof1373;
case 1373:
	switch( (*( p)) ) {
		case 76: goto st1374;
		case 108: goto st1374;
	}
	goto tr261;
st1374:
	if ( ++( p) == ( pe) )
		goto _test_eof1374;
case 1374:
	switch( (*( p)) ) {
		case 69: goto st1375;
		case 101: goto st1375;
	}
	goto tr261;
st1375:
	if ( ++( p) == ( pe) )
		goto _test_eof1375;
case 1375:
	switch( (*( p)) ) {
		case 82: goto st1376;
		case 114: goto st1376;
	}
	goto tr261;
st1376:
	if ( ++( p) == ( pe) )
		goto _test_eof1376;
case 1376:
	switch( (*( p)) ) {
		case 62: goto tr1321;
		case 83: goto st1377;
		case 115: goto st1377;
	}
	goto tr261;
st1377:
	if ( ++( p) == ( pe) )
		goto _test_eof1377;
case 1377:
	if ( (*( p)) == 62 )
		goto tr1321;
	goto tr261;
st1378:
	if ( ++( p) == ( pe) )
		goto _test_eof1378;
case 1378:
	switch( (*( p)) ) {
		case 82: goto st1379;
		case 114: goto st1379;
	}
	goto tr261;
st1379:
	if ( ++( p) == ( pe) )
		goto _test_eof1379;
case 1379:
	switch( (*( p)) ) {
		case 79: goto st1380;
		case 111: goto st1380;
	}
	goto tr261;
st1380:
	if ( ++( p) == ( pe) )
		goto _test_eof1380;
case 1380:
	switch( (*( p)) ) {
		case 78: goto st1381;
		case 110: goto st1381;
	}
	goto tr261;
st1381:
	if ( ++( p) == ( pe) )
		goto _test_eof1381;
case 1381:
	switch( (*( p)) ) {
		case 71: goto st1382;
		case 103: goto st1382;
	}
	goto tr261;
st1382:
	if ( ++( p) == ( pe) )
		goto _test_eof1382;
case 1382:
	if ( (*( p)) == 62 )
		goto tr1086;
	goto tr261;
st1383:
	if ( ++( p) == ( pe) )
		goto _test_eof1383;
case 1383:
	switch( (*( p)) ) {
		case 66: goto st1384;
		case 80: goto st1385;
		case 98: goto st1384;
		case 112: goto st1385;
	}
	goto tr261;
st1384:
	if ( ++( p) == ( pe) )
		goto _test_eof1384;
case 1384:
	if ( (*( p)) == 62 )
		goto tr1664;
	goto tr261;
st1385:
	if ( ++( p) == ( pe) )
		goto _test_eof1385;
case 1385:
	if ( (*( p)) == 62 )
		goto tr1665;
	goto tr261;
st1386:
	if ( ++( p) == ( pe) )
		goto _test_eof1386;
case 1386:
	switch( (*( p)) ) {
		case 78: goto st1387;
		case 110: goto st1387;
	}
	goto tr261;
st1387:
	if ( ++( p) == ( pe) )
		goto _test_eof1387;
case 1387:
	if ( (*( p)) == 62 )
		goto tr1336;
	goto tr261;
st1388:
	if ( ++( p) == ( pe) )
		goto _test_eof1388;
case 1388:
	if ( (*( p)) == 62 )
		goto tr1338;
	goto tr261;
tr2175:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1934;
st1934:
	if ( ++( p) == ( pe) )
		goto _test_eof1934;
case 1934:
#line 28810 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( 64 <= (*( p)) && (*( p)) <= 64 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 47: goto st1306;
		case 65: goto st1320;
		case 66: goto st1343;
		case 67: goto st1345;
		case 69: goto st1352;
		case 72: goto tr2469;
		case 73: goto st1353;
		case 78: goto st1363;
		case 81: goto st254;
		case 83: goto st1370;
		case 84: goto st1386;
		case 85: goto st1388;
		case 97: goto st1320;
		case 98: goto st1343;
		case 99: goto st1345;
		case 101: goto st1352;
		case 104: goto tr2469;
		case 105: goto st1353;
		case 110: goto st1363;
		case 113: goto st254;
		case 115: goto st1370;
		case 116: goto st1386;
		case 117: goto st1388;
		case 1088: goto st1389;
	}
	goto tr2177;
st1389:
	if ( ++( p) == ( pe) )
		goto _test_eof1389;
case 1389:
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
			goto tr1667;
	} else if ( _widec > 1032 ) {
		if ( _widec > 1055 ) {
			if ( 1057 <= _widec && _widec <= 1151 )
				goto tr1667;
		} else if ( _widec >= 1038 )
			goto tr1667;
	} else
		goto tr1667;
	goto tr261;
tr1668:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1390;
tr1667:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1390;
st1390:
	if ( ++( p) == ( pe) )
		goto _test_eof1390;
case 1390:
#line 28903 "ext/dtext/dtext.cpp"
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
		goto tr1669;
	if ( _widec < 1025 ) {
		if ( 896 <= _widec && _widec <= 1023 )
			goto tr1668;
	} else if ( _widec > 1033 ) {
		if ( _widec > 1036 ) {
			if ( 1038 <= _widec && _widec <= 1151 )
				goto tr1668;
		} else if ( _widec >= 1035 )
			goto tr1668;
	} else
		goto tr1668;
	goto tr261;
tr2176:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1935;
st1935:
	if ( ++( p) == ( pe) )
		goto _test_eof1935;
case 1935:
#line 28970 "ext/dtext/dtext.cpp"
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
		case 994: goto tr2477;
		case 995: goto tr2478;
		case 1007: goto tr2479;
		case 1070: goto tr2482;
		case 1119: goto tr2482;
		case 1151: goto tr2481;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( 962 <= _widec && _widec <= 991 )
				goto tr2475;
		} else if ( _widec > 1006 ) {
			if ( 1008 <= _widec && _widec <= 1012 )
				goto tr2480;
		} else
			goto tr2476;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( 1038 <= _widec && _widec <= 1055 )
				goto tr2481;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr2481;
			} else if ( _widec >= 1089 )
				goto tr2481;
		} else
			goto tr2481;
	} else
		goto tr2481;
	goto tr2177;
tr2475:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1391;
st1391:
	if ( ++( p) == ( pe) )
		goto _test_eof1391;
case 1391:
#line 29116 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1392;
	goto tr261;
tr2481:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1392;
st1392:
	if ( ++( p) == ( pe) )
		goto _test_eof1392;
case 1392:
#line 29135 "ext/dtext/dtext.cpp"
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
		case 994: goto st1406;
		case 995: goto st1408;
		case 1007: goto st1410;
		case 1057: goto st1392;
		case 1063: goto st1414;
		case 1067: goto st1392;
		case 1119: goto st1392;
		case 1151: goto tr1678;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1404;
			} else if ( _widec >= 896 )
				goto st1393;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1393;
			} else if ( _widec >= 1008 )
				goto st1413;
		} else
			goto st1405;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1392;
			} else if ( _widec >= 1038 )
				goto tr1678;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1678;
			} else if ( _widec >= 1089 )
				goto tr1678;
		} else
			goto tr1678;
	} else
		goto tr1678;
	goto tr254;
st1393:
	if ( ++( p) == ( pe) )
		goto _test_eof1393;
case 1393:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1394;
			} else if ( _widec >= 896 )
				goto st1393;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1393;
			} else if ( _widec >= 1008 )
				goto st1402;
		} else
			goto st1395;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1393;
			} else if ( _widec >= 1038 )
				goto tr1686;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else if ( _widec >= 1089 )
				goto tr1686;
		} else
			goto tr1686;
	} else
		goto tr1686;
	goto tr254;
st1394:
	if ( ++( p) == ( pe) )
		goto _test_eof1394;
case 1394:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto tr1686;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
tr1686:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 418 "ext/dtext/dtext.cpp.rl"
	{( act) = 58;}
	goto st1936;
st1936:
	if ( ++( p) == ( pe) )
		goto _test_eof1936;
case 1936:
#line 29715 "ext/dtext/dtext.cpp"
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1394;
			} else if ( _widec >= 896 )
				goto st1393;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1393;
			} else if ( _widec >= 1008 )
				goto st1402;
		} else
			goto st1395;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1393;
			} else if ( _widec >= 1038 )
				goto tr1686;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else if ( _widec >= 1089 )
				goto tr1686;
		} else
			goto tr1686;
	} else
		goto tr1686;
	goto tr2483;
st1395:
	if ( ++( p) == ( pe) )
		goto _test_eof1395;
case 1395:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto st1394;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1396:
	if ( ++( p) == ( pe) )
		goto _test_eof1396;
case 1396:
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
		case 925: goto st1397;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto st1394;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1397:
	if ( ++( p) == ( pe) )
		goto _test_eof1397;
case 1397:
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
		case 941: goto st1393;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto tr1686;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1398:
	if ( ++( p) == ( pe) )
		goto _test_eof1398;
case 1398:
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
		case 896: goto st1399;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 897 )
				goto st1394;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1399:
	if ( ++( p) == ( pe) )
		goto _test_eof1399;
case 1399:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 992 ) {
		if ( _widec < 914 ) {
			if ( _widec < 899 ) {
				if ( 896 <= _widec && _widec <= 898 )
					goto st1393;
			} else if ( _widec > 903 ) {
				if ( 904 <= _widec && _widec <= 913 )
					goto st1393;
			} else
				goto tr1686;
		} else if ( _widec > 915 ) {
			if ( _widec < 925 ) {
				if ( 916 <= _widec && _widec <= 924 )
					goto st1393;
			} else if ( _widec > 959 ) {
				if ( _widec > 961 ) {
					if ( 962 <= _widec && _widec <= 991 )
						goto st1394;
				} else if ( _widec >= 960 )
					goto st1393;
			} else
				goto tr1686;
		} else
			goto tr1686;
	} else if ( _widec > 1006 ) {
		if ( _widec < 1038 ) {
			if ( _widec < 1013 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec > 1023 ) {
				if ( 1025 <= _widec && _widec <= 1032 )
					goto tr1686;
			} else
				goto st1393;
		} else if ( _widec > 1055 ) {
			if ( _widec < 1072 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1393;
			} else if ( _widec > 1081 ) {
				if ( _widec > 1114 ) {
					if ( 1121 <= _widec && _widec <= 1146 )
						goto tr1686;
				} else if ( _widec >= 1089 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto tr1686;
	} else
		goto st1395;
	goto tr254;
st1400:
	if ( ++( p) == ( pe) )
		goto _test_eof1400;
case 1400:
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
		case 956: goto st1401;
		case 957: goto st1403;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto st1394;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1401:
	if ( ++( p) == ( pe) )
		goto _test_eof1401;
case 1401:
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
		case 905: goto st1393;
		case 957: goto st1393;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto tr1686;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1402:
	if ( ++( p) == ( pe) )
		goto _test_eof1402;
case 1402:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto st1395;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1403:
	if ( ++( p) == ( pe) )
		goto _test_eof1403;
case 1403:
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
		case 925: goto st1393;
		case 928: goto st1393;
		case 931: goto st1393;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto tr1686;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1404:
	if ( ++( p) == ( pe) )
		goto _test_eof1404;
case 1404:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto tr1678;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
tr1678:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 418 "ext/dtext/dtext.cpp.rl"
	{( act) = 58;}
	goto st1937;
st1937:
	if ( ++( p) == ( pe) )
		goto _test_eof1937;
case 1937:
#line 32067 "ext/dtext/dtext.cpp"
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
		case 994: goto st1406;
		case 995: goto st1408;
		case 1007: goto st1410;
		case 1057: goto st1392;
		case 1063: goto st1414;
		case 1067: goto st1392;
		case 1119: goto st1392;
		case 1151: goto tr1678;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1404;
			} else if ( _widec >= 896 )
				goto st1393;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1393;
			} else if ( _widec >= 1008 )
				goto st1413;
		} else
			goto st1405;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1392;
			} else if ( _widec >= 1038 )
				goto tr1678;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1678;
			} else if ( _widec >= 1089 )
				goto tr1678;
		} else
			goto tr1678;
	} else
		goto tr1678;
	goto tr2483;
st1405:
	if ( ++( p) == ( pe) )
		goto _test_eof1405;
case 1405:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto st1404;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1406:
	if ( ++( p) == ( pe) )
		goto _test_eof1406;
case 1406:
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
		case 925: goto st1407;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto st1404;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1407:
	if ( ++( p) == ( pe) )
		goto _test_eof1407;
case 1407:
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
		case 941: goto st1392;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto tr1678;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1408:
	if ( ++( p) == ( pe) )
		goto _test_eof1408;
case 1408:
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
		case 896: goto st1409;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 897 )
				goto st1404;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1409:
	if ( ++( p) == ( pe) )
		goto _test_eof1409;
case 1409:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 992 ) {
		if ( _widec < 914 ) {
			if ( _widec < 899 ) {
				if ( 896 <= _widec && _widec <= 898 )
					goto st1392;
			} else if ( _widec > 903 ) {
				if ( 904 <= _widec && _widec <= 913 )
					goto st1392;
			} else
				goto tr1678;
		} else if ( _widec > 915 ) {
			if ( _widec < 925 ) {
				if ( 916 <= _widec && _widec <= 924 )
					goto st1392;
			} else if ( _widec > 959 ) {
				if ( _widec > 961 ) {
					if ( 962 <= _widec && _widec <= 991 )
						goto st1394;
				} else if ( _widec >= 960 )
					goto st1393;
			} else
				goto tr1678;
		} else
			goto tr1678;
	} else if ( _widec > 1006 ) {
		if ( _widec < 1038 ) {
			if ( _widec < 1013 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec > 1023 ) {
				if ( 1025 <= _widec && _widec <= 1032 )
					goto tr1686;
			} else
				goto st1393;
		} else if ( _widec > 1055 ) {
			if ( _widec < 1072 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1393;
			} else if ( _widec > 1081 ) {
				if ( _widec > 1114 ) {
					if ( 1121 <= _widec && _widec <= 1146 )
						goto tr1686;
				} else if ( _widec >= 1089 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto tr1686;
	} else
		goto st1395;
	goto tr254;
st1410:
	if ( ++( p) == ( pe) )
		goto _test_eof1410;
case 1410:
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
		case 956: goto st1411;
		case 957: goto st1412;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto st1404;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1411:
	if ( ++( p) == ( pe) )
		goto _test_eof1411;
case 1411:
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
		case 905: goto st1392;
		case 957: goto st1392;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto tr1678;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1412:
	if ( ++( p) == ( pe) )
		goto _test_eof1412;
case 1412:
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
		case 925: goto st1392;
		case 928: goto st1392;
		case 931: goto st1392;
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto tr1678;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1413:
	if ( ++( p) == ( pe) )
		goto _test_eof1413;
case 1413:
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
		case 994: goto st1396;
		case 995: goto st1398;
		case 1007: goto st1400;
		case 1057: goto st1393;
		case 1063: goto st1393;
		case 1067: goto st1393;
		case 1119: goto st1393;
		case 1151: goto tr1686;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1393;
			} else if ( _widec >= 896 )
				goto st1405;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1402;
			} else if ( _widec >= 992 )
				goto st1395;
		} else
			goto st1394;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1686;
			} else if ( _widec >= 1025 )
				goto tr1686;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1686;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1686;
			} else
				goto tr1686;
		} else
			goto st1393;
	} else
		goto st1393;
	goto tr254;
st1414:
	if ( ++( p) == ( pe) )
		goto _test_eof1414;
case 1414:
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
		case 994: goto st1406;
		case 995: goto st1408;
		case 1007: goto st1410;
		case 1057: goto st1392;
		case 1063: goto st1414;
		case 1067: goto st1392;
		case 1119: goto st1392;
		case 1124: goto st1392;
		case 1139: goto st1392;
		case 1151: goto tr1678;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1404;
			} else if ( _widec >= 896 )
				goto st1393;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1393;
			} else if ( _widec >= 1008 )
				goto st1413;
		} else
			goto st1405;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1392;
			} else if ( _widec >= 1038 )
				goto tr1678;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1678;
			} else if ( _widec >= 1089 )
				goto tr1678;
		} else
			goto tr1678;
	} else
		goto tr1678;
	goto tr254;
tr2476:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1415;
st1415:
	if ( ++( p) == ( pe) )
		goto _test_eof1415;
case 1415:
#line 34435 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1391;
	goto tr261;
tr2477:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1416;
st1416:
	if ( ++( p) == ( pe) )
		goto _test_eof1416;
case 1416:
#line 34454 "ext/dtext/dtext.cpp"
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
		goto st1417;
	if ( 896 <= _widec && _widec <= 959 )
		goto st1391;
	goto tr261;
st1417:
	if ( ++( p) == ( pe) )
		goto _test_eof1417;
case 1417:
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
			goto st1392;
	} else if ( _widec >= 896 )
		goto st1392;
	goto tr261;
tr2478:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1418;
st1418:
	if ( ++( p) == ( pe) )
		goto _test_eof1418;
case 1418:
#line 34513 "ext/dtext/dtext.cpp"
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
		goto st1419;
	if ( 897 <= _widec && _widec <= 959 )
		goto st1391;
	goto tr261;
st1419:
	if ( ++( p) == ( pe) )
		goto _test_eof1419;
case 1419:
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
			goto st1392;
	} else if ( _widec > 915 ) {
		if ( 925 <= _widec && _widec <= 959 )
			goto st1392;
	} else
		goto st1392;
	goto tr261;
tr2479:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1420;
st1420:
	if ( ++( p) == ( pe) )
		goto _test_eof1420;
case 1420:
#line 34575 "ext/dtext/dtext.cpp"
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
		case 956: goto st1421;
		case 957: goto st1422;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1391;
	goto tr261;
st1421:
	if ( ++( p) == ( pe) )
		goto _test_eof1421;
case 1421:
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
			goto st1392;
	} else if ( _widec > 956 ) {
		if ( 958 <= _widec && _widec <= 959 )
			goto st1392;
	} else
		goto st1392;
	goto tr261;
st1422:
	if ( ++( p) == ( pe) )
		goto _test_eof1422;
case 1422:
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
			goto st1392;
	} else if ( _widec > 927 ) {
		if ( _widec > 930 ) {
			if ( 932 <= _widec && _widec <= 959 )
				goto st1392;
		} else if ( _widec >= 929 )
			goto st1392;
	} else
		goto st1392;
	goto tr261;
tr2480:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1423;
st1423:
	if ( ++( p) == ( pe) )
		goto _test_eof1423;
case 1423:
#line 34697 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1415;
	goto tr261;
tr2482:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1424;
st1424:
	if ( ++( p) == ( pe) )
		goto _test_eof1424;
case 1424:
#line 34716 "ext/dtext/dtext.cpp"
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
		case 994: goto st1416;
		case 995: goto st1418;
		case 1007: goto st1420;
		case 1151: goto st1392;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( 962 <= _widec && _widec <= 991 )
				goto st1391;
		} else if ( _widec > 1006 ) {
			if ( 1008 <= _widec && _widec <= 1012 )
				goto st1423;
		} else
			goto st1415;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( 1038 <= _widec && _widec <= 1055 )
				goto st1392;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto st1392;
			} else if ( _widec >= 1089 )
				goto st1392;
		} else
			goto st1392;
	} else
		goto st1392;
	goto tr261;
tr1705:
#line 642 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1938;
tr1711:
#line 635 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_rewind();
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1938;
tr2484:
#line 642 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1938;
tr2485:
#line 640 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1938;
tr2489:
#line 642 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1938;
st1938:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1938;
case 1938:
#line 1 "NONE"
	{( ts) = ( p);}
#line 34875 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2485;
		case 10: goto tr2486;
		case 60: goto tr2487;
		case 91: goto tr2488;
	}
	goto tr2484;
tr2486:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1939;
st1939:
	if ( ++( p) == ( pe) )
		goto _test_eof1939;
case 1939:
#line 34891 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto st1425;
		case 91: goto st1431;
	}
	goto tr2489;
st1425:
	if ( ++( p) == ( pe) )
		goto _test_eof1425;
case 1425:
	if ( (*( p)) == 47 )
		goto st1426;
	goto tr1705;
st1426:
	if ( ++( p) == ( pe) )
		goto _test_eof1426;
case 1426:
	switch( (*( p)) ) {
		case 67: goto st1427;
		case 99: goto st1427;
	}
	goto tr1705;
st1427:
	if ( ++( p) == ( pe) )
		goto _test_eof1427;
case 1427:
	switch( (*( p)) ) {
		case 79: goto st1428;
		case 111: goto st1428;
	}
	goto tr1705;
st1428:
	if ( ++( p) == ( pe) )
		goto _test_eof1428;
case 1428:
	switch( (*( p)) ) {
		case 68: goto st1429;
		case 100: goto st1429;
	}
	goto tr1705;
st1429:
	if ( ++( p) == ( pe) )
		goto _test_eof1429;
case 1429:
	switch( (*( p)) ) {
		case 69: goto st1430;
		case 101: goto st1430;
	}
	goto tr1705;
st1430:
	if ( ++( p) == ( pe) )
		goto _test_eof1430;
case 1430:
	if ( (*( p)) == 62 )
		goto tr1711;
	goto tr1705;
st1431:
	if ( ++( p) == ( pe) )
		goto _test_eof1431;
case 1431:
	if ( (*( p)) == 47 )
		goto st1432;
	goto tr1705;
st1432:
	if ( ++( p) == ( pe) )
		goto _test_eof1432;
case 1432:
	switch( (*( p)) ) {
		case 67: goto st1433;
		case 99: goto st1433;
	}
	goto tr1705;
st1433:
	if ( ++( p) == ( pe) )
		goto _test_eof1433;
case 1433:
	switch( (*( p)) ) {
		case 79: goto st1434;
		case 111: goto st1434;
	}
	goto tr1705;
st1434:
	if ( ++( p) == ( pe) )
		goto _test_eof1434;
case 1434:
	switch( (*( p)) ) {
		case 68: goto st1435;
		case 100: goto st1435;
	}
	goto tr1705;
st1435:
	if ( ++( p) == ( pe) )
		goto _test_eof1435;
case 1435:
	switch( (*( p)) ) {
		case 69: goto st1436;
		case 101: goto st1436;
	}
	goto tr1705;
st1436:
	if ( ++( p) == ( pe) )
		goto _test_eof1436;
case 1436:
	if ( (*( p)) == 93 )
		goto tr1711;
	goto tr1705;
tr2487:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1940;
st1940:
	if ( ++( p) == ( pe) )
		goto _test_eof1940;
case 1940:
#line 35005 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1426;
	goto tr2489;
tr2488:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1941;
st1941:
	if ( ++( p) == ( pe) )
		goto _test_eof1941;
case 1941:
#line 35017 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1432;
	goto tr2489;
tr1717:
#line 655 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1942;
tr1726:
#line 648 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_rewind();
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1942;
tr2492:
#line 655 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1942;
tr2493:
#line 653 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1942;
tr2497:
#line 655 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1942;
st1942:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1942;
case 1942:
#line 1 "NONE"
	{( ts) = ( p);}
#line 35058 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2493;
		case 10: goto tr2494;
		case 60: goto tr2495;
		case 91: goto tr2496;
	}
	goto tr2492;
tr2494:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1943;
st1943:
	if ( ++( p) == ( pe) )
		goto _test_eof1943;
case 1943:
#line 35074 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto st1437;
		case 91: goto st1446;
	}
	goto tr2497;
st1437:
	if ( ++( p) == ( pe) )
		goto _test_eof1437;
case 1437:
	if ( (*( p)) == 47 )
		goto st1438;
	goto tr1717;
st1438:
	if ( ++( p) == ( pe) )
		goto _test_eof1438;
case 1438:
	switch( (*( p)) ) {
		case 78: goto st1439;
		case 110: goto st1439;
	}
	goto tr1717;
st1439:
	if ( ++( p) == ( pe) )
		goto _test_eof1439;
case 1439:
	switch( (*( p)) ) {
		case 79: goto st1440;
		case 111: goto st1440;
	}
	goto tr1717;
st1440:
	if ( ++( p) == ( pe) )
		goto _test_eof1440;
case 1440:
	switch( (*( p)) ) {
		case 68: goto st1441;
		case 100: goto st1441;
	}
	goto tr1717;
st1441:
	if ( ++( p) == ( pe) )
		goto _test_eof1441;
case 1441:
	switch( (*( p)) ) {
		case 84: goto st1442;
		case 116: goto st1442;
	}
	goto tr1717;
st1442:
	if ( ++( p) == ( pe) )
		goto _test_eof1442;
case 1442:
	switch( (*( p)) ) {
		case 69: goto st1443;
		case 101: goto st1443;
	}
	goto tr1717;
st1443:
	if ( ++( p) == ( pe) )
		goto _test_eof1443;
case 1443:
	switch( (*( p)) ) {
		case 88: goto st1444;
		case 120: goto st1444;
	}
	goto tr1717;
st1444:
	if ( ++( p) == ( pe) )
		goto _test_eof1444;
case 1444:
	switch( (*( p)) ) {
		case 84: goto st1445;
		case 116: goto st1445;
	}
	goto tr1717;
st1445:
	if ( ++( p) == ( pe) )
		goto _test_eof1445;
case 1445:
	if ( (*( p)) == 62 )
		goto tr1726;
	goto tr1717;
st1446:
	if ( ++( p) == ( pe) )
		goto _test_eof1446;
case 1446:
	if ( (*( p)) == 47 )
		goto st1447;
	goto tr1717;
st1447:
	if ( ++( p) == ( pe) )
		goto _test_eof1447;
case 1447:
	switch( (*( p)) ) {
		case 78: goto st1448;
		case 110: goto st1448;
	}
	goto tr1717;
st1448:
	if ( ++( p) == ( pe) )
		goto _test_eof1448;
case 1448:
	switch( (*( p)) ) {
		case 79: goto st1449;
		case 111: goto st1449;
	}
	goto tr1717;
st1449:
	if ( ++( p) == ( pe) )
		goto _test_eof1449;
case 1449:
	switch( (*( p)) ) {
		case 68: goto st1450;
		case 100: goto st1450;
	}
	goto tr1717;
st1450:
	if ( ++( p) == ( pe) )
		goto _test_eof1450;
case 1450:
	switch( (*( p)) ) {
		case 84: goto st1451;
		case 116: goto st1451;
	}
	goto tr1717;
st1451:
	if ( ++( p) == ( pe) )
		goto _test_eof1451;
case 1451:
	switch( (*( p)) ) {
		case 69: goto st1452;
		case 101: goto st1452;
	}
	goto tr1717;
st1452:
	if ( ++( p) == ( pe) )
		goto _test_eof1452;
case 1452:
	switch( (*( p)) ) {
		case 88: goto st1453;
		case 120: goto st1453;
	}
	goto tr1717;
st1453:
	if ( ++( p) == ( pe) )
		goto _test_eof1453;
case 1453:
	switch( (*( p)) ) {
		case 84: goto st1454;
		case 116: goto st1454;
	}
	goto tr1717;
st1454:
	if ( ++( p) == ( pe) )
		goto _test_eof1454;
case 1454:
	if ( (*( p)) == 93 )
		goto tr1726;
	goto tr1717;
tr2495:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1944;
st1944:
	if ( ++( p) == ( pe) )
		goto _test_eof1944;
case 1944:
#line 35242 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1438;
	goto tr2497;
tr2496:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1945;
st1945:
	if ( ++( p) == ( pe) )
		goto _test_eof1945;
case 1945:
#line 35254 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1447;
	goto tr2497;
tr1735:
#line 714 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}}
	goto st1946;
tr1745:
#line 665 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_COLGROUP, { ts, te });
  }}
	goto st1946;
tr1753:
#line 708 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TABLE, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1946;
tr1757:
#line 686 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_TBODY, { ts, te });
  }}
	goto st1946;
tr1761:
#line 678 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_THEAD, { ts, te });
  }}
	goto st1946;
tr1762:
#line 699 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_TR, { ts, te });
  }}
	goto st1946;
tr1766:
#line 669 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1946;
tr1781:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 669 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1946;
tr1786:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 669 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1946;
tr1792:
#line 661 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1946;
tr1806:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 661 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1946;
tr1811:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 661 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1946;
tr1820:
#line 682 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1946;
tr1834:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 682 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1946;
tr1839:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 682 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1946;
tr1841:
#line 703 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1946;goto st1708;}}
  }}
	goto st1946;
tr1855:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 703 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1946;goto st1708;}}
  }}
	goto st1946;
tr1860:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 703 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1946;goto st1708;}}
  }}
	goto st1946;
tr1862:
#line 690 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1946;goto st1708;}}
  }}
	goto st1946;
tr1877:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 690 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1946;goto st1708;}}
  }}
	goto st1946;
tr1882:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 690 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1946;goto st1708;}}
  }}
	goto st1946;
tr1886:
#line 674 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1946;
tr1900:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 674 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1946;
tr1905:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 674 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1946;
tr1907:
#line 695 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1946;
tr1921:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 695 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1946;
tr1926:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 695 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1946;
tr2500:
#line 714 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1946;
tr2503:
#line 714 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;}
	goto st1946;
st1946:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1946;
case 1946:
#line 1 "NONE"
	{( ts) = ( p);}
#line 35559 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto tr2501;
		case 91: goto tr2502;
	}
	goto tr2500;
tr2501:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1947;
st1947:
	if ( ++( p) == ( pe) )
		goto _test_eof1947;
case 1947:
#line 35573 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1455;
		case 67: goto st1478;
		case 84: goto st1506;
		case 99: goto st1478;
		case 116: goto st1506;
	}
	goto tr2503;
st1455:
	if ( ++( p) == ( pe) )
		goto _test_eof1455;
case 1455:
	switch( (*( p)) ) {
		case 67: goto st1456;
		case 84: goto st1464;
		case 99: goto st1456;
		case 116: goto st1464;
	}
	goto tr1735;
st1456:
	if ( ++( p) == ( pe) )
		goto _test_eof1456;
case 1456:
	switch( (*( p)) ) {
		case 79: goto st1457;
		case 111: goto st1457;
	}
	goto tr1735;
st1457:
	if ( ++( p) == ( pe) )
		goto _test_eof1457;
case 1457:
	switch( (*( p)) ) {
		case 76: goto st1458;
		case 108: goto st1458;
	}
	goto tr1735;
st1458:
	if ( ++( p) == ( pe) )
		goto _test_eof1458;
case 1458:
	switch( (*( p)) ) {
		case 71: goto st1459;
		case 103: goto st1459;
	}
	goto tr1735;
st1459:
	if ( ++( p) == ( pe) )
		goto _test_eof1459;
case 1459:
	switch( (*( p)) ) {
		case 82: goto st1460;
		case 114: goto st1460;
	}
	goto tr1735;
st1460:
	if ( ++( p) == ( pe) )
		goto _test_eof1460;
case 1460:
	switch( (*( p)) ) {
		case 79: goto st1461;
		case 111: goto st1461;
	}
	goto tr1735;
st1461:
	if ( ++( p) == ( pe) )
		goto _test_eof1461;
case 1461:
	switch( (*( p)) ) {
		case 85: goto st1462;
		case 117: goto st1462;
	}
	goto tr1735;
st1462:
	if ( ++( p) == ( pe) )
		goto _test_eof1462;
case 1462:
	switch( (*( p)) ) {
		case 80: goto st1463;
		case 112: goto st1463;
	}
	goto tr1735;
st1463:
	if ( ++( p) == ( pe) )
		goto _test_eof1463;
case 1463:
	if ( (*( p)) == 62 )
		goto tr1745;
	goto tr1735;
st1464:
	if ( ++( p) == ( pe) )
		goto _test_eof1464;
case 1464:
	switch( (*( p)) ) {
		case 65: goto st1465;
		case 66: goto st1469;
		case 72: goto st1473;
		case 82: goto st1477;
		case 97: goto st1465;
		case 98: goto st1469;
		case 104: goto st1473;
		case 114: goto st1477;
	}
	goto tr1735;
st1465:
	if ( ++( p) == ( pe) )
		goto _test_eof1465;
case 1465:
	switch( (*( p)) ) {
		case 66: goto st1466;
		case 98: goto st1466;
	}
	goto tr1735;
st1466:
	if ( ++( p) == ( pe) )
		goto _test_eof1466;
case 1466:
	switch( (*( p)) ) {
		case 76: goto st1467;
		case 108: goto st1467;
	}
	goto tr1735;
st1467:
	if ( ++( p) == ( pe) )
		goto _test_eof1467;
case 1467:
	switch( (*( p)) ) {
		case 69: goto st1468;
		case 101: goto st1468;
	}
	goto tr1735;
st1468:
	if ( ++( p) == ( pe) )
		goto _test_eof1468;
case 1468:
	if ( (*( p)) == 62 )
		goto tr1753;
	goto tr1735;
st1469:
	if ( ++( p) == ( pe) )
		goto _test_eof1469;
case 1469:
	switch( (*( p)) ) {
		case 79: goto st1470;
		case 111: goto st1470;
	}
	goto tr1735;
st1470:
	if ( ++( p) == ( pe) )
		goto _test_eof1470;
case 1470:
	switch( (*( p)) ) {
		case 68: goto st1471;
		case 100: goto st1471;
	}
	goto tr1735;
st1471:
	if ( ++( p) == ( pe) )
		goto _test_eof1471;
case 1471:
	switch( (*( p)) ) {
		case 89: goto st1472;
		case 121: goto st1472;
	}
	goto tr1735;
st1472:
	if ( ++( p) == ( pe) )
		goto _test_eof1472;
case 1472:
	if ( (*( p)) == 62 )
		goto tr1757;
	goto tr1735;
st1473:
	if ( ++( p) == ( pe) )
		goto _test_eof1473;
case 1473:
	switch( (*( p)) ) {
		case 69: goto st1474;
		case 101: goto st1474;
	}
	goto tr1735;
st1474:
	if ( ++( p) == ( pe) )
		goto _test_eof1474;
case 1474:
	switch( (*( p)) ) {
		case 65: goto st1475;
		case 97: goto st1475;
	}
	goto tr1735;
st1475:
	if ( ++( p) == ( pe) )
		goto _test_eof1475;
case 1475:
	switch( (*( p)) ) {
		case 68: goto st1476;
		case 100: goto st1476;
	}
	goto tr1735;
st1476:
	if ( ++( p) == ( pe) )
		goto _test_eof1476;
case 1476:
	if ( (*( p)) == 62 )
		goto tr1761;
	goto tr1735;
st1477:
	if ( ++( p) == ( pe) )
		goto _test_eof1477;
case 1477:
	if ( (*( p)) == 62 )
		goto tr1762;
	goto tr1735;
st1478:
	if ( ++( p) == ( pe) )
		goto _test_eof1478;
case 1478:
	switch( (*( p)) ) {
		case 79: goto st1479;
		case 111: goto st1479;
	}
	goto tr1735;
st1479:
	if ( ++( p) == ( pe) )
		goto _test_eof1479;
case 1479:
	switch( (*( p)) ) {
		case 76: goto st1480;
		case 108: goto st1480;
	}
	goto tr1735;
st1480:
	if ( ++( p) == ( pe) )
		goto _test_eof1480;
case 1480:
	switch( (*( p)) ) {
		case 9: goto st1481;
		case 32: goto st1481;
		case 62: goto tr1766;
		case 71: goto st1491;
		case 103: goto st1491;
	}
	goto tr1735;
tr1780:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1481;
tr1784:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1481;
st1481:
	if ( ++( p) == ( pe) )
		goto _test_eof1481;
case 1481:
#line 35831 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1481;
		case 32: goto st1481;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1768;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1768;
	} else
		goto tr1768;
	goto tr1735;
tr1768:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1482;
st1482:
	if ( ++( p) == ( pe) )
		goto _test_eof1482;
case 1482:
#line 35853 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1769;
		case 32: goto tr1769;
		case 61: goto tr1771;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1482;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1482;
	} else
		goto st1482;
	goto tr1735;
tr1769:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1483;
st1483:
	if ( ++( p) == ( pe) )
		goto _test_eof1483;
case 1483:
#line 35876 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1483;
		case 32: goto st1483;
		case 61: goto st1484;
	}
	goto tr1735;
tr1771:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1484;
st1484:
	if ( ++( p) == ( pe) )
		goto _test_eof1484;
case 1484:
#line 35891 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1484;
		case 32: goto st1484;
		case 34: goto st1485;
		case 39: goto st1488;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1776;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1776;
	} else
		goto tr1776;
	goto tr1735;
st1485:
	if ( ++( p) == ( pe) )
		goto _test_eof1485;
case 1485:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1777;
tr1777:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1486;
st1486:
	if ( ++( p) == ( pe) )
		goto _test_eof1486;
case 1486:
#line 35925 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1779;
	}
	goto st1486;
tr1779:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1487;
st1487:
	if ( ++( p) == ( pe) )
		goto _test_eof1487;
case 1487:
#line 35941 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1780;
		case 32: goto tr1780;
		case 62: goto tr1781;
	}
	goto tr1735;
st1488:
	if ( ++( p) == ( pe) )
		goto _test_eof1488;
case 1488:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1782;
tr1782:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1489;
st1489:
	if ( ++( p) == ( pe) )
		goto _test_eof1489;
case 1489:
#line 35966 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1779;
	}
	goto st1489;
tr1776:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1490;
st1490:
	if ( ++( p) == ( pe) )
		goto _test_eof1490;
case 1490:
#line 35982 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1784;
		case 32: goto tr1784;
		case 62: goto tr1786;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1490;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1490;
	} else
		goto st1490;
	goto tr1735;
st1491:
	if ( ++( p) == ( pe) )
		goto _test_eof1491;
case 1491:
	switch( (*( p)) ) {
		case 82: goto st1492;
		case 114: goto st1492;
	}
	goto tr1735;
st1492:
	if ( ++( p) == ( pe) )
		goto _test_eof1492;
case 1492:
	switch( (*( p)) ) {
		case 79: goto st1493;
		case 111: goto st1493;
	}
	goto tr1735;
st1493:
	if ( ++( p) == ( pe) )
		goto _test_eof1493;
case 1493:
	switch( (*( p)) ) {
		case 85: goto st1494;
		case 117: goto st1494;
	}
	goto tr1735;
st1494:
	if ( ++( p) == ( pe) )
		goto _test_eof1494;
case 1494:
	switch( (*( p)) ) {
		case 80: goto st1495;
		case 112: goto st1495;
	}
	goto tr1735;
st1495:
	if ( ++( p) == ( pe) )
		goto _test_eof1495;
case 1495:
	switch( (*( p)) ) {
		case 9: goto st1496;
		case 32: goto st1496;
		case 62: goto tr1792;
	}
	goto tr1735;
tr1805:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1496;
tr1809:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1496;
st1496:
	if ( ++( p) == ( pe) )
		goto _test_eof1496;
case 1496:
#line 36057 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1496;
		case 32: goto st1496;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1793;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1793;
	} else
		goto tr1793;
	goto tr1735;
tr1793:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1497;
st1497:
	if ( ++( p) == ( pe) )
		goto _test_eof1497;
case 1497:
#line 36079 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1794;
		case 32: goto tr1794;
		case 61: goto tr1796;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1497;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1497;
	} else
		goto st1497;
	goto tr1735;
tr1794:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1498;
st1498:
	if ( ++( p) == ( pe) )
		goto _test_eof1498;
case 1498:
#line 36102 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1498;
		case 32: goto st1498;
		case 61: goto st1499;
	}
	goto tr1735;
tr1796:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1499;
st1499:
	if ( ++( p) == ( pe) )
		goto _test_eof1499;
case 1499:
#line 36117 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1499;
		case 32: goto st1499;
		case 34: goto st1500;
		case 39: goto st1503;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1801;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1801;
	} else
		goto tr1801;
	goto tr1735;
st1500:
	if ( ++( p) == ( pe) )
		goto _test_eof1500;
case 1500:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1802;
tr1802:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1501;
st1501:
	if ( ++( p) == ( pe) )
		goto _test_eof1501;
case 1501:
#line 36151 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1804;
	}
	goto st1501;
tr1804:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1502;
st1502:
	if ( ++( p) == ( pe) )
		goto _test_eof1502;
case 1502:
#line 36167 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1805;
		case 32: goto tr1805;
		case 62: goto tr1806;
	}
	goto tr1735;
st1503:
	if ( ++( p) == ( pe) )
		goto _test_eof1503;
case 1503:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1807;
tr1807:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1504;
st1504:
	if ( ++( p) == ( pe) )
		goto _test_eof1504;
case 1504:
#line 36192 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1804;
	}
	goto st1504;
tr1801:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1505;
st1505:
	if ( ++( p) == ( pe) )
		goto _test_eof1505;
case 1505:
#line 36208 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1809;
		case 32: goto tr1809;
		case 62: goto tr1811;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1505;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1505;
	} else
		goto st1505;
	goto tr1735;
st1506:
	if ( ++( p) == ( pe) )
		goto _test_eof1506;
case 1506:
	switch( (*( p)) ) {
		case 66: goto st1507;
		case 68: goto st1521;
		case 72: goto st1532;
		case 82: goto st1556;
		case 98: goto st1507;
		case 100: goto st1521;
		case 104: goto st1532;
		case 114: goto st1556;
	}
	goto tr1735;
st1507:
	if ( ++( p) == ( pe) )
		goto _test_eof1507;
case 1507:
	switch( (*( p)) ) {
		case 79: goto st1508;
		case 111: goto st1508;
	}
	goto tr1735;
st1508:
	if ( ++( p) == ( pe) )
		goto _test_eof1508;
case 1508:
	switch( (*( p)) ) {
		case 68: goto st1509;
		case 100: goto st1509;
	}
	goto tr1735;
st1509:
	if ( ++( p) == ( pe) )
		goto _test_eof1509;
case 1509:
	switch( (*( p)) ) {
		case 89: goto st1510;
		case 121: goto st1510;
	}
	goto tr1735;
st1510:
	if ( ++( p) == ( pe) )
		goto _test_eof1510;
case 1510:
	switch( (*( p)) ) {
		case 9: goto st1511;
		case 32: goto st1511;
		case 62: goto tr1820;
	}
	goto tr1735;
tr1833:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1511;
tr1837:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1511;
st1511:
	if ( ++( p) == ( pe) )
		goto _test_eof1511;
case 1511:
#line 36289 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1511;
		case 32: goto st1511;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1821;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1821;
	} else
		goto tr1821;
	goto tr1735;
tr1821:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1512;
st1512:
	if ( ++( p) == ( pe) )
		goto _test_eof1512;
case 1512:
#line 36311 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1822;
		case 32: goto tr1822;
		case 61: goto tr1824;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1512;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1512;
	} else
		goto st1512;
	goto tr1735;
tr1822:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1513;
st1513:
	if ( ++( p) == ( pe) )
		goto _test_eof1513;
case 1513:
#line 36334 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1513;
		case 32: goto st1513;
		case 61: goto st1514;
	}
	goto tr1735;
tr1824:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1514;
st1514:
	if ( ++( p) == ( pe) )
		goto _test_eof1514;
case 1514:
#line 36349 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1514;
		case 32: goto st1514;
		case 34: goto st1515;
		case 39: goto st1518;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1829;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1829;
	} else
		goto tr1829;
	goto tr1735;
st1515:
	if ( ++( p) == ( pe) )
		goto _test_eof1515;
case 1515:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1830;
tr1830:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1516;
st1516:
	if ( ++( p) == ( pe) )
		goto _test_eof1516;
case 1516:
#line 36383 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1832;
	}
	goto st1516;
tr1832:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1517;
st1517:
	if ( ++( p) == ( pe) )
		goto _test_eof1517;
case 1517:
#line 36399 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1833;
		case 32: goto tr1833;
		case 62: goto tr1834;
	}
	goto tr1735;
st1518:
	if ( ++( p) == ( pe) )
		goto _test_eof1518;
case 1518:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1835;
tr1835:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1519;
st1519:
	if ( ++( p) == ( pe) )
		goto _test_eof1519;
case 1519:
#line 36424 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1832;
	}
	goto st1519;
tr1829:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1520;
st1520:
	if ( ++( p) == ( pe) )
		goto _test_eof1520;
case 1520:
#line 36440 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1837;
		case 32: goto tr1837;
		case 62: goto tr1839;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1520;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1520;
	} else
		goto st1520;
	goto tr1735;
st1521:
	if ( ++( p) == ( pe) )
		goto _test_eof1521;
case 1521:
	switch( (*( p)) ) {
		case 9: goto st1522;
		case 32: goto st1522;
		case 62: goto tr1841;
	}
	goto tr1735;
tr1854:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1522;
tr1858:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1522;
st1522:
	if ( ++( p) == ( pe) )
		goto _test_eof1522;
case 1522:
#line 36479 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1522;
		case 32: goto st1522;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1842;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1842;
	} else
		goto tr1842;
	goto tr1735;
tr1842:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1523;
st1523:
	if ( ++( p) == ( pe) )
		goto _test_eof1523;
case 1523:
#line 36501 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1843;
		case 32: goto tr1843;
		case 61: goto tr1845;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1523;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1523;
	} else
		goto st1523;
	goto tr1735;
tr1843:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1524;
st1524:
	if ( ++( p) == ( pe) )
		goto _test_eof1524;
case 1524:
#line 36524 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1524;
		case 32: goto st1524;
		case 61: goto st1525;
	}
	goto tr1735;
tr1845:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1525;
st1525:
	if ( ++( p) == ( pe) )
		goto _test_eof1525;
case 1525:
#line 36539 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1525;
		case 32: goto st1525;
		case 34: goto st1526;
		case 39: goto st1529;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1850;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1850;
	} else
		goto tr1850;
	goto tr1735;
st1526:
	if ( ++( p) == ( pe) )
		goto _test_eof1526;
case 1526:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1851;
tr1851:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1527;
st1527:
	if ( ++( p) == ( pe) )
		goto _test_eof1527;
case 1527:
#line 36573 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1853;
	}
	goto st1527;
tr1853:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1528;
st1528:
	if ( ++( p) == ( pe) )
		goto _test_eof1528;
case 1528:
#line 36589 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1854;
		case 32: goto tr1854;
		case 62: goto tr1855;
	}
	goto tr1735;
st1529:
	if ( ++( p) == ( pe) )
		goto _test_eof1529;
case 1529:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1856;
tr1856:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1530;
st1530:
	if ( ++( p) == ( pe) )
		goto _test_eof1530;
case 1530:
#line 36614 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1853;
	}
	goto st1530;
tr1850:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1531;
st1531:
	if ( ++( p) == ( pe) )
		goto _test_eof1531;
case 1531:
#line 36630 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1858;
		case 32: goto tr1858;
		case 62: goto tr1860;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1531;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1531;
	} else
		goto st1531;
	goto tr1735;
st1532:
	if ( ++( p) == ( pe) )
		goto _test_eof1532;
case 1532:
	switch( (*( p)) ) {
		case 9: goto st1533;
		case 32: goto st1533;
		case 62: goto tr1862;
		case 69: goto st1543;
		case 101: goto st1543;
	}
	goto tr1735;
tr1876:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1533;
tr1880:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1533;
st1533:
	if ( ++( p) == ( pe) )
		goto _test_eof1533;
case 1533:
#line 36671 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1533;
		case 32: goto st1533;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1864;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1864;
	} else
		goto tr1864;
	goto tr1735;
tr1864:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1534;
st1534:
	if ( ++( p) == ( pe) )
		goto _test_eof1534;
case 1534:
#line 36693 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1865;
		case 32: goto tr1865;
		case 61: goto tr1867;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1534;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1534;
	} else
		goto st1534;
	goto tr1735;
tr1865:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1535;
st1535:
	if ( ++( p) == ( pe) )
		goto _test_eof1535;
case 1535:
#line 36716 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1535;
		case 32: goto st1535;
		case 61: goto st1536;
	}
	goto tr1735;
tr1867:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1536;
st1536:
	if ( ++( p) == ( pe) )
		goto _test_eof1536;
case 1536:
#line 36731 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1536;
		case 32: goto st1536;
		case 34: goto st1537;
		case 39: goto st1540;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1872;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1872;
	} else
		goto tr1872;
	goto tr1735;
st1537:
	if ( ++( p) == ( pe) )
		goto _test_eof1537;
case 1537:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1873;
tr1873:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1538;
st1538:
	if ( ++( p) == ( pe) )
		goto _test_eof1538;
case 1538:
#line 36765 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1875;
	}
	goto st1538;
tr1875:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1539;
st1539:
	if ( ++( p) == ( pe) )
		goto _test_eof1539;
case 1539:
#line 36781 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1876;
		case 32: goto tr1876;
		case 62: goto tr1877;
	}
	goto tr1735;
st1540:
	if ( ++( p) == ( pe) )
		goto _test_eof1540;
case 1540:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1878;
tr1878:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1541;
st1541:
	if ( ++( p) == ( pe) )
		goto _test_eof1541;
case 1541:
#line 36806 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1875;
	}
	goto st1541;
tr1872:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1542;
st1542:
	if ( ++( p) == ( pe) )
		goto _test_eof1542;
case 1542:
#line 36822 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1880;
		case 32: goto tr1880;
		case 62: goto tr1882;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1542;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1542;
	} else
		goto st1542;
	goto tr1735;
st1543:
	if ( ++( p) == ( pe) )
		goto _test_eof1543;
case 1543:
	switch( (*( p)) ) {
		case 65: goto st1544;
		case 97: goto st1544;
	}
	goto tr1735;
st1544:
	if ( ++( p) == ( pe) )
		goto _test_eof1544;
case 1544:
	switch( (*( p)) ) {
		case 68: goto st1545;
		case 100: goto st1545;
	}
	goto tr1735;
st1545:
	if ( ++( p) == ( pe) )
		goto _test_eof1545;
case 1545:
	switch( (*( p)) ) {
		case 9: goto st1546;
		case 32: goto st1546;
		case 62: goto tr1886;
	}
	goto tr1735;
tr1899:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1546;
tr1903:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1546;
st1546:
	if ( ++( p) == ( pe) )
		goto _test_eof1546;
case 1546:
#line 36879 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1546;
		case 32: goto st1546;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1887;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1887;
	} else
		goto tr1887;
	goto tr1735;
tr1887:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1547;
st1547:
	if ( ++( p) == ( pe) )
		goto _test_eof1547;
case 1547:
#line 36901 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1888;
		case 32: goto tr1888;
		case 61: goto tr1890;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1547;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1547;
	} else
		goto st1547;
	goto tr1735;
tr1888:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1548;
st1548:
	if ( ++( p) == ( pe) )
		goto _test_eof1548;
case 1548:
#line 36924 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1548;
		case 32: goto st1548;
		case 61: goto st1549;
	}
	goto tr1735;
tr1890:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1549;
st1549:
	if ( ++( p) == ( pe) )
		goto _test_eof1549;
case 1549:
#line 36939 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1549;
		case 32: goto st1549;
		case 34: goto st1550;
		case 39: goto st1553;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1895;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1895;
	} else
		goto tr1895;
	goto tr1735;
st1550:
	if ( ++( p) == ( pe) )
		goto _test_eof1550;
case 1550:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1896;
tr1896:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1551;
st1551:
	if ( ++( p) == ( pe) )
		goto _test_eof1551;
case 1551:
#line 36973 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1898;
	}
	goto st1551;
tr1898:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1552;
st1552:
	if ( ++( p) == ( pe) )
		goto _test_eof1552;
case 1552:
#line 36989 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1899;
		case 32: goto tr1899;
		case 62: goto tr1900;
	}
	goto tr1735;
st1553:
	if ( ++( p) == ( pe) )
		goto _test_eof1553;
case 1553:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1901;
tr1901:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1554;
st1554:
	if ( ++( p) == ( pe) )
		goto _test_eof1554;
case 1554:
#line 37014 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1898;
	}
	goto st1554;
tr1895:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1555;
st1555:
	if ( ++( p) == ( pe) )
		goto _test_eof1555;
case 1555:
#line 37030 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1903;
		case 32: goto tr1903;
		case 62: goto tr1905;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1555;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1555;
	} else
		goto st1555;
	goto tr1735;
st1556:
	if ( ++( p) == ( pe) )
		goto _test_eof1556;
case 1556:
	switch( (*( p)) ) {
		case 9: goto st1557;
		case 32: goto st1557;
		case 62: goto tr1907;
	}
	goto tr1735;
tr1920:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1557;
tr1924:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1557;
st1557:
	if ( ++( p) == ( pe) )
		goto _test_eof1557;
case 1557:
#line 37069 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1557;
		case 32: goto st1557;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1908;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1908;
	} else
		goto tr1908;
	goto tr1735;
tr1908:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1558;
st1558:
	if ( ++( p) == ( pe) )
		goto _test_eof1558;
case 1558:
#line 37091 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1909;
		case 32: goto tr1909;
		case 61: goto tr1911;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1558;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1558;
	} else
		goto st1558;
	goto tr1735;
tr1909:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1559;
st1559:
	if ( ++( p) == ( pe) )
		goto _test_eof1559;
case 1559:
#line 37114 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1559;
		case 32: goto st1559;
		case 61: goto st1560;
	}
	goto tr1735;
tr1911:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1560;
st1560:
	if ( ++( p) == ( pe) )
		goto _test_eof1560;
case 1560:
#line 37129 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1560;
		case 32: goto st1560;
		case 34: goto st1561;
		case 39: goto st1564;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1916;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1916;
	} else
		goto tr1916;
	goto tr1735;
st1561:
	if ( ++( p) == ( pe) )
		goto _test_eof1561;
case 1561:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1917;
tr1917:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1562;
st1562:
	if ( ++( p) == ( pe) )
		goto _test_eof1562;
case 1562:
#line 37163 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1919;
	}
	goto st1562;
tr1919:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1563;
st1563:
	if ( ++( p) == ( pe) )
		goto _test_eof1563;
case 1563:
#line 37179 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1920;
		case 32: goto tr1920;
		case 62: goto tr1921;
	}
	goto tr1735;
st1564:
	if ( ++( p) == ( pe) )
		goto _test_eof1564;
case 1564:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1922;
tr1922:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1565;
st1565:
	if ( ++( p) == ( pe) )
		goto _test_eof1565;
case 1565:
#line 37204 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1919;
	}
	goto st1565;
tr1916:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1566;
st1566:
	if ( ++( p) == ( pe) )
		goto _test_eof1566;
case 1566:
#line 37220 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1924;
		case 32: goto tr1924;
		case 62: goto tr1926;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1566;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1566;
	} else
		goto st1566;
	goto tr1735;
tr2502:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1948;
st1948:
	if ( ++( p) == ( pe) )
		goto _test_eof1948;
case 1948:
#line 37243 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1567;
		case 67: goto st1590;
		case 84: goto st1618;
		case 99: goto st1590;
		case 116: goto st1618;
	}
	goto tr2503;
st1567:
	if ( ++( p) == ( pe) )
		goto _test_eof1567;
case 1567:
	switch( (*( p)) ) {
		case 67: goto st1568;
		case 84: goto st1576;
		case 99: goto st1568;
		case 116: goto st1576;
	}
	goto tr1735;
st1568:
	if ( ++( p) == ( pe) )
		goto _test_eof1568;
case 1568:
	switch( (*( p)) ) {
		case 79: goto st1569;
		case 111: goto st1569;
	}
	goto tr1735;
st1569:
	if ( ++( p) == ( pe) )
		goto _test_eof1569;
case 1569:
	switch( (*( p)) ) {
		case 76: goto st1570;
		case 108: goto st1570;
	}
	goto tr1735;
st1570:
	if ( ++( p) == ( pe) )
		goto _test_eof1570;
case 1570:
	switch( (*( p)) ) {
		case 71: goto st1571;
		case 103: goto st1571;
	}
	goto tr1735;
st1571:
	if ( ++( p) == ( pe) )
		goto _test_eof1571;
case 1571:
	switch( (*( p)) ) {
		case 82: goto st1572;
		case 114: goto st1572;
	}
	goto tr1735;
st1572:
	if ( ++( p) == ( pe) )
		goto _test_eof1572;
case 1572:
	switch( (*( p)) ) {
		case 79: goto st1573;
		case 111: goto st1573;
	}
	goto tr1735;
st1573:
	if ( ++( p) == ( pe) )
		goto _test_eof1573;
case 1573:
	switch( (*( p)) ) {
		case 85: goto st1574;
		case 117: goto st1574;
	}
	goto tr1735;
st1574:
	if ( ++( p) == ( pe) )
		goto _test_eof1574;
case 1574:
	switch( (*( p)) ) {
		case 80: goto st1575;
		case 112: goto st1575;
	}
	goto tr1735;
st1575:
	if ( ++( p) == ( pe) )
		goto _test_eof1575;
case 1575:
	if ( (*( p)) == 93 )
		goto tr1745;
	goto tr1735;
st1576:
	if ( ++( p) == ( pe) )
		goto _test_eof1576;
case 1576:
	switch( (*( p)) ) {
		case 65: goto st1577;
		case 66: goto st1581;
		case 72: goto st1585;
		case 82: goto st1589;
		case 97: goto st1577;
		case 98: goto st1581;
		case 104: goto st1585;
		case 114: goto st1589;
	}
	goto tr1735;
st1577:
	if ( ++( p) == ( pe) )
		goto _test_eof1577;
case 1577:
	switch( (*( p)) ) {
		case 66: goto st1578;
		case 98: goto st1578;
	}
	goto tr1735;
st1578:
	if ( ++( p) == ( pe) )
		goto _test_eof1578;
case 1578:
	switch( (*( p)) ) {
		case 76: goto st1579;
		case 108: goto st1579;
	}
	goto tr1735;
st1579:
	if ( ++( p) == ( pe) )
		goto _test_eof1579;
case 1579:
	switch( (*( p)) ) {
		case 69: goto st1580;
		case 101: goto st1580;
	}
	goto tr1735;
st1580:
	if ( ++( p) == ( pe) )
		goto _test_eof1580;
case 1580:
	if ( (*( p)) == 93 )
		goto tr1753;
	goto tr1735;
st1581:
	if ( ++( p) == ( pe) )
		goto _test_eof1581;
case 1581:
	switch( (*( p)) ) {
		case 79: goto st1582;
		case 111: goto st1582;
	}
	goto tr1735;
st1582:
	if ( ++( p) == ( pe) )
		goto _test_eof1582;
case 1582:
	switch( (*( p)) ) {
		case 68: goto st1583;
		case 100: goto st1583;
	}
	goto tr1735;
st1583:
	if ( ++( p) == ( pe) )
		goto _test_eof1583;
case 1583:
	switch( (*( p)) ) {
		case 89: goto st1584;
		case 121: goto st1584;
	}
	goto tr1735;
st1584:
	if ( ++( p) == ( pe) )
		goto _test_eof1584;
case 1584:
	if ( (*( p)) == 93 )
		goto tr1757;
	goto tr1735;
st1585:
	if ( ++( p) == ( pe) )
		goto _test_eof1585;
case 1585:
	switch( (*( p)) ) {
		case 69: goto st1586;
		case 101: goto st1586;
	}
	goto tr1735;
st1586:
	if ( ++( p) == ( pe) )
		goto _test_eof1586;
case 1586:
	switch( (*( p)) ) {
		case 65: goto st1587;
		case 97: goto st1587;
	}
	goto tr1735;
st1587:
	if ( ++( p) == ( pe) )
		goto _test_eof1587;
case 1587:
	switch( (*( p)) ) {
		case 68: goto st1588;
		case 100: goto st1588;
	}
	goto tr1735;
st1588:
	if ( ++( p) == ( pe) )
		goto _test_eof1588;
case 1588:
	if ( (*( p)) == 93 )
		goto tr1761;
	goto tr1735;
st1589:
	if ( ++( p) == ( pe) )
		goto _test_eof1589;
case 1589:
	if ( (*( p)) == 93 )
		goto tr1762;
	goto tr1735;
st1590:
	if ( ++( p) == ( pe) )
		goto _test_eof1590;
case 1590:
	switch( (*( p)) ) {
		case 79: goto st1591;
		case 111: goto st1591;
	}
	goto tr1735;
st1591:
	if ( ++( p) == ( pe) )
		goto _test_eof1591;
case 1591:
	switch( (*( p)) ) {
		case 76: goto st1592;
		case 108: goto st1592;
	}
	goto tr1735;
st1592:
	if ( ++( p) == ( pe) )
		goto _test_eof1592;
case 1592:
	switch( (*( p)) ) {
		case 9: goto st1593;
		case 32: goto st1593;
		case 71: goto st1603;
		case 93: goto tr1766;
		case 103: goto st1603;
	}
	goto tr1735;
tr1965:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1593;
tr1968:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1593;
st1593:
	if ( ++( p) == ( pe) )
		goto _test_eof1593;
case 1593:
#line 37501 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1593;
		case 32: goto st1593;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1953;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1953;
	} else
		goto tr1953;
	goto tr1735;
tr1953:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1594;
st1594:
	if ( ++( p) == ( pe) )
		goto _test_eof1594;
case 1594:
#line 37523 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1954;
		case 32: goto tr1954;
		case 61: goto tr1956;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1594;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1594;
	} else
		goto st1594;
	goto tr1735;
tr1954:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1595;
st1595:
	if ( ++( p) == ( pe) )
		goto _test_eof1595;
case 1595:
#line 37546 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1595;
		case 32: goto st1595;
		case 61: goto st1596;
	}
	goto tr1735;
tr1956:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1596;
st1596:
	if ( ++( p) == ( pe) )
		goto _test_eof1596;
case 1596:
#line 37561 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1596;
		case 32: goto st1596;
		case 34: goto st1597;
		case 39: goto st1600;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1961;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1961;
	} else
		goto tr1961;
	goto tr1735;
st1597:
	if ( ++( p) == ( pe) )
		goto _test_eof1597;
case 1597:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1962;
tr1962:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1598;
st1598:
	if ( ++( p) == ( pe) )
		goto _test_eof1598;
case 1598:
#line 37595 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1964;
	}
	goto st1598;
tr1964:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1599;
st1599:
	if ( ++( p) == ( pe) )
		goto _test_eof1599;
case 1599:
#line 37611 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1965;
		case 32: goto tr1965;
		case 93: goto tr1781;
	}
	goto tr1735;
st1600:
	if ( ++( p) == ( pe) )
		goto _test_eof1600;
case 1600:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1966;
tr1966:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1601;
st1601:
	if ( ++( p) == ( pe) )
		goto _test_eof1601;
case 1601:
#line 37636 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1964;
	}
	goto st1601;
tr1961:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1602;
st1602:
	if ( ++( p) == ( pe) )
		goto _test_eof1602;
case 1602:
#line 37652 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1968;
		case 32: goto tr1968;
		case 93: goto tr1786;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1602;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1602;
	} else
		goto st1602;
	goto tr1735;
st1603:
	if ( ++( p) == ( pe) )
		goto _test_eof1603;
case 1603:
	switch( (*( p)) ) {
		case 82: goto st1604;
		case 114: goto st1604;
	}
	goto tr1735;
st1604:
	if ( ++( p) == ( pe) )
		goto _test_eof1604;
case 1604:
	switch( (*( p)) ) {
		case 79: goto st1605;
		case 111: goto st1605;
	}
	goto tr1735;
st1605:
	if ( ++( p) == ( pe) )
		goto _test_eof1605;
case 1605:
	switch( (*( p)) ) {
		case 85: goto st1606;
		case 117: goto st1606;
	}
	goto tr1735;
st1606:
	if ( ++( p) == ( pe) )
		goto _test_eof1606;
case 1606:
	switch( (*( p)) ) {
		case 80: goto st1607;
		case 112: goto st1607;
	}
	goto tr1735;
st1607:
	if ( ++( p) == ( pe) )
		goto _test_eof1607;
case 1607:
	switch( (*( p)) ) {
		case 9: goto st1608;
		case 32: goto st1608;
		case 93: goto tr1792;
	}
	goto tr1735;
tr1987:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1608;
tr1990:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1608;
st1608:
	if ( ++( p) == ( pe) )
		goto _test_eof1608;
case 1608:
#line 37727 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1608;
		case 32: goto st1608;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1975;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1975;
	} else
		goto tr1975;
	goto tr1735;
tr1975:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1609;
st1609:
	if ( ++( p) == ( pe) )
		goto _test_eof1609;
case 1609:
#line 37749 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1976;
		case 32: goto tr1976;
		case 61: goto tr1978;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1609;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1609;
	} else
		goto st1609;
	goto tr1735;
tr1976:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1610;
st1610:
	if ( ++( p) == ( pe) )
		goto _test_eof1610;
case 1610:
#line 37772 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1610;
		case 32: goto st1610;
		case 61: goto st1611;
	}
	goto tr1735;
tr1978:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1611;
st1611:
	if ( ++( p) == ( pe) )
		goto _test_eof1611;
case 1611:
#line 37787 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1611;
		case 32: goto st1611;
		case 34: goto st1612;
		case 39: goto st1615;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1983;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1983;
	} else
		goto tr1983;
	goto tr1735;
st1612:
	if ( ++( p) == ( pe) )
		goto _test_eof1612;
case 1612:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1984;
tr1984:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1613;
st1613:
	if ( ++( p) == ( pe) )
		goto _test_eof1613;
case 1613:
#line 37821 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr1986;
	}
	goto st1613;
tr1986:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1614;
st1614:
	if ( ++( p) == ( pe) )
		goto _test_eof1614;
case 1614:
#line 37837 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1987;
		case 32: goto tr1987;
		case 93: goto tr1806;
	}
	goto tr1735;
st1615:
	if ( ++( p) == ( pe) )
		goto _test_eof1615;
case 1615:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr1988;
tr1988:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1616;
st1616:
	if ( ++( p) == ( pe) )
		goto _test_eof1616;
case 1616:
#line 37862 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr1986;
	}
	goto st1616;
tr1983:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1617;
st1617:
	if ( ++( p) == ( pe) )
		goto _test_eof1617;
case 1617:
#line 37878 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1990;
		case 32: goto tr1990;
		case 93: goto tr1811;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1617;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1617;
	} else
		goto st1617;
	goto tr1735;
st1618:
	if ( ++( p) == ( pe) )
		goto _test_eof1618;
case 1618:
	switch( (*( p)) ) {
		case 66: goto st1619;
		case 68: goto st1633;
		case 72: goto st1644;
		case 82: goto st1668;
		case 98: goto st1619;
		case 100: goto st1633;
		case 104: goto st1644;
		case 114: goto st1668;
	}
	goto tr1735;
st1619:
	if ( ++( p) == ( pe) )
		goto _test_eof1619;
case 1619:
	switch( (*( p)) ) {
		case 79: goto st1620;
		case 111: goto st1620;
	}
	goto tr1735;
st1620:
	if ( ++( p) == ( pe) )
		goto _test_eof1620;
case 1620:
	switch( (*( p)) ) {
		case 68: goto st1621;
		case 100: goto st1621;
	}
	goto tr1735;
st1621:
	if ( ++( p) == ( pe) )
		goto _test_eof1621;
case 1621:
	switch( (*( p)) ) {
		case 89: goto st1622;
		case 121: goto st1622;
	}
	goto tr1735;
st1622:
	if ( ++( p) == ( pe) )
		goto _test_eof1622;
case 1622:
	switch( (*( p)) ) {
		case 9: goto st1623;
		case 32: goto st1623;
		case 93: goto tr1820;
	}
	goto tr1735;
tr2012:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1623;
tr2015:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1623;
st1623:
	if ( ++( p) == ( pe) )
		goto _test_eof1623;
case 1623:
#line 37959 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1623;
		case 32: goto st1623;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2000;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2000;
	} else
		goto tr2000;
	goto tr1735;
tr2000:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1624;
st1624:
	if ( ++( p) == ( pe) )
		goto _test_eof1624;
case 1624:
#line 37981 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2001;
		case 32: goto tr2001;
		case 61: goto tr2003;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1624;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1624;
	} else
		goto st1624;
	goto tr1735;
tr2001:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1625;
st1625:
	if ( ++( p) == ( pe) )
		goto _test_eof1625;
case 1625:
#line 38004 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1625;
		case 32: goto st1625;
		case 61: goto st1626;
	}
	goto tr1735;
tr2003:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1626;
st1626:
	if ( ++( p) == ( pe) )
		goto _test_eof1626;
case 1626:
#line 38019 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1626;
		case 32: goto st1626;
		case 34: goto st1627;
		case 39: goto st1630;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2008;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2008;
	} else
		goto tr2008;
	goto tr1735;
st1627:
	if ( ++( p) == ( pe) )
		goto _test_eof1627;
case 1627:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2009;
tr2009:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1628;
st1628:
	if ( ++( p) == ( pe) )
		goto _test_eof1628;
case 1628:
#line 38053 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr2011;
	}
	goto st1628;
tr2011:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1629;
st1629:
	if ( ++( p) == ( pe) )
		goto _test_eof1629;
case 1629:
#line 38069 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2012;
		case 32: goto tr2012;
		case 93: goto tr1834;
	}
	goto tr1735;
st1630:
	if ( ++( p) == ( pe) )
		goto _test_eof1630;
case 1630:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2013;
tr2013:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1631;
st1631:
	if ( ++( p) == ( pe) )
		goto _test_eof1631;
case 1631:
#line 38094 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr2011;
	}
	goto st1631;
tr2008:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1632;
st1632:
	if ( ++( p) == ( pe) )
		goto _test_eof1632;
case 1632:
#line 38110 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2015;
		case 32: goto tr2015;
		case 93: goto tr1839;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1632;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1632;
	} else
		goto st1632;
	goto tr1735;
st1633:
	if ( ++( p) == ( pe) )
		goto _test_eof1633;
case 1633:
	switch( (*( p)) ) {
		case 9: goto st1634;
		case 32: goto st1634;
		case 93: goto tr1841;
	}
	goto tr1735;
tr2030:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1634;
tr2033:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1634;
st1634:
	if ( ++( p) == ( pe) )
		goto _test_eof1634;
case 1634:
#line 38149 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1634;
		case 32: goto st1634;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2018;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2018;
	} else
		goto tr2018;
	goto tr1735;
tr2018:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1635;
st1635:
	if ( ++( p) == ( pe) )
		goto _test_eof1635;
case 1635:
#line 38171 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2019;
		case 32: goto tr2019;
		case 61: goto tr2021;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1635;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1635;
	} else
		goto st1635;
	goto tr1735;
tr2019:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1636;
st1636:
	if ( ++( p) == ( pe) )
		goto _test_eof1636;
case 1636:
#line 38194 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1636;
		case 32: goto st1636;
		case 61: goto st1637;
	}
	goto tr1735;
tr2021:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1637;
st1637:
	if ( ++( p) == ( pe) )
		goto _test_eof1637;
case 1637:
#line 38209 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1637;
		case 32: goto st1637;
		case 34: goto st1638;
		case 39: goto st1641;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2026;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2026;
	} else
		goto tr2026;
	goto tr1735;
st1638:
	if ( ++( p) == ( pe) )
		goto _test_eof1638;
case 1638:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2027;
tr2027:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1639;
st1639:
	if ( ++( p) == ( pe) )
		goto _test_eof1639;
case 1639:
#line 38243 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr2029;
	}
	goto st1639;
tr2029:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1640;
st1640:
	if ( ++( p) == ( pe) )
		goto _test_eof1640;
case 1640:
#line 38259 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2030;
		case 32: goto tr2030;
		case 93: goto tr1855;
	}
	goto tr1735;
st1641:
	if ( ++( p) == ( pe) )
		goto _test_eof1641;
case 1641:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2031;
tr2031:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1642;
st1642:
	if ( ++( p) == ( pe) )
		goto _test_eof1642;
case 1642:
#line 38284 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr2029;
	}
	goto st1642;
tr2026:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1643;
st1643:
	if ( ++( p) == ( pe) )
		goto _test_eof1643;
case 1643:
#line 38300 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2033;
		case 32: goto tr2033;
		case 93: goto tr1860;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1643;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1643;
	} else
		goto st1643;
	goto tr1735;
st1644:
	if ( ++( p) == ( pe) )
		goto _test_eof1644;
case 1644:
	switch( (*( p)) ) {
		case 9: goto st1645;
		case 32: goto st1645;
		case 69: goto st1655;
		case 93: goto tr1862;
		case 101: goto st1655;
	}
	goto tr1735;
tr2049:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1645;
tr2052:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1645;
st1645:
	if ( ++( p) == ( pe) )
		goto _test_eof1645;
case 1645:
#line 38341 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1645;
		case 32: goto st1645;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2037;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2037;
	} else
		goto tr2037;
	goto tr1735;
tr2037:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1646;
st1646:
	if ( ++( p) == ( pe) )
		goto _test_eof1646;
case 1646:
#line 38363 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2038;
		case 32: goto tr2038;
		case 61: goto tr2040;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1646;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1646;
	} else
		goto st1646;
	goto tr1735;
tr2038:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1647;
st1647:
	if ( ++( p) == ( pe) )
		goto _test_eof1647;
case 1647:
#line 38386 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1647;
		case 32: goto st1647;
		case 61: goto st1648;
	}
	goto tr1735;
tr2040:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1648;
st1648:
	if ( ++( p) == ( pe) )
		goto _test_eof1648;
case 1648:
#line 38401 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1648;
		case 32: goto st1648;
		case 34: goto st1649;
		case 39: goto st1652;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2045;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2045;
	} else
		goto tr2045;
	goto tr1735;
st1649:
	if ( ++( p) == ( pe) )
		goto _test_eof1649;
case 1649:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2046;
tr2046:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1650;
st1650:
	if ( ++( p) == ( pe) )
		goto _test_eof1650;
case 1650:
#line 38435 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr2048;
	}
	goto st1650;
tr2048:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1651;
st1651:
	if ( ++( p) == ( pe) )
		goto _test_eof1651;
case 1651:
#line 38451 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2049;
		case 32: goto tr2049;
		case 93: goto tr1877;
	}
	goto tr1735;
st1652:
	if ( ++( p) == ( pe) )
		goto _test_eof1652;
case 1652:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2050;
tr2050:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1653;
st1653:
	if ( ++( p) == ( pe) )
		goto _test_eof1653;
case 1653:
#line 38476 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr2048;
	}
	goto st1653;
tr2045:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1654;
st1654:
	if ( ++( p) == ( pe) )
		goto _test_eof1654;
case 1654:
#line 38492 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2052;
		case 32: goto tr2052;
		case 93: goto tr1882;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1654;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1654;
	} else
		goto st1654;
	goto tr1735;
st1655:
	if ( ++( p) == ( pe) )
		goto _test_eof1655;
case 1655:
	switch( (*( p)) ) {
		case 65: goto st1656;
		case 97: goto st1656;
	}
	goto tr1735;
st1656:
	if ( ++( p) == ( pe) )
		goto _test_eof1656;
case 1656:
	switch( (*( p)) ) {
		case 68: goto st1657;
		case 100: goto st1657;
	}
	goto tr1735;
st1657:
	if ( ++( p) == ( pe) )
		goto _test_eof1657;
case 1657:
	switch( (*( p)) ) {
		case 9: goto st1658;
		case 32: goto st1658;
		case 93: goto tr1886;
	}
	goto tr1735;
tr2069:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1658;
tr2072:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1658;
st1658:
	if ( ++( p) == ( pe) )
		goto _test_eof1658;
case 1658:
#line 38549 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1658;
		case 32: goto st1658;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2057;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2057;
	} else
		goto tr2057;
	goto tr1735;
tr2057:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1659;
st1659:
	if ( ++( p) == ( pe) )
		goto _test_eof1659;
case 1659:
#line 38571 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2058;
		case 32: goto tr2058;
		case 61: goto tr2060;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1659;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1659;
	} else
		goto st1659;
	goto tr1735;
tr2058:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1660;
st1660:
	if ( ++( p) == ( pe) )
		goto _test_eof1660;
case 1660:
#line 38594 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1660;
		case 32: goto st1660;
		case 61: goto st1661;
	}
	goto tr1735;
tr2060:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1661;
st1661:
	if ( ++( p) == ( pe) )
		goto _test_eof1661;
case 1661:
#line 38609 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1661;
		case 32: goto st1661;
		case 34: goto st1662;
		case 39: goto st1665;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2065;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2065;
	} else
		goto tr2065;
	goto tr1735;
st1662:
	if ( ++( p) == ( pe) )
		goto _test_eof1662;
case 1662:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2066;
tr2066:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1663;
st1663:
	if ( ++( p) == ( pe) )
		goto _test_eof1663;
case 1663:
#line 38643 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr2068;
	}
	goto st1663;
tr2068:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1664;
st1664:
	if ( ++( p) == ( pe) )
		goto _test_eof1664;
case 1664:
#line 38659 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2069;
		case 32: goto tr2069;
		case 93: goto tr1900;
	}
	goto tr1735;
st1665:
	if ( ++( p) == ( pe) )
		goto _test_eof1665;
case 1665:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2070;
tr2070:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1666;
st1666:
	if ( ++( p) == ( pe) )
		goto _test_eof1666;
case 1666:
#line 38684 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr2068;
	}
	goto st1666;
tr2065:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1667;
st1667:
	if ( ++( p) == ( pe) )
		goto _test_eof1667;
case 1667:
#line 38700 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2072;
		case 32: goto tr2072;
		case 93: goto tr1905;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1667;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1667;
	} else
		goto st1667;
	goto tr1735;
st1668:
	if ( ++( p) == ( pe) )
		goto _test_eof1668;
case 1668:
	switch( (*( p)) ) {
		case 9: goto st1669;
		case 32: goto st1669;
		case 93: goto tr1907;
	}
	goto tr1735;
tr2087:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1669;
tr2090:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1669;
st1669:
	if ( ++( p) == ( pe) )
		goto _test_eof1669;
case 1669:
#line 38739 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1669;
		case 32: goto st1669;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2075;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2075;
	} else
		goto tr2075;
	goto tr1735;
tr2075:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1670;
st1670:
	if ( ++( p) == ( pe) )
		goto _test_eof1670;
case 1670:
#line 38761 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2076;
		case 32: goto tr2076;
		case 61: goto tr2078;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1670;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1670;
	} else
		goto st1670;
	goto tr1735;
tr2076:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1671;
st1671:
	if ( ++( p) == ( pe) )
		goto _test_eof1671;
case 1671:
#line 38784 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1671;
		case 32: goto st1671;
		case 61: goto st1672;
	}
	goto tr1735;
tr2078:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1672;
st1672:
	if ( ++( p) == ( pe) )
		goto _test_eof1672;
case 1672:
#line 38799 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1672;
		case 32: goto st1672;
		case 34: goto st1673;
		case 39: goto st1676;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2083;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2083;
	} else
		goto tr2083;
	goto tr1735;
st1673:
	if ( ++( p) == ( pe) )
		goto _test_eof1673;
case 1673:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2084;
tr2084:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1674;
st1674:
	if ( ++( p) == ( pe) )
		goto _test_eof1674;
case 1674:
#line 38833 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 34: goto tr2086;
	}
	goto st1674;
tr2086:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1675;
st1675:
	if ( ++( p) == ( pe) )
		goto _test_eof1675;
case 1675:
#line 38849 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2087;
		case 32: goto tr2087;
		case 93: goto tr1921;
	}
	goto tr1735;
st1676:
	if ( ++( p) == ( pe) )
		goto _test_eof1676;
case 1676:
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
	}
	goto tr2088;
tr2088:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1677;
st1677:
	if ( ++( p) == ( pe) )
		goto _test_eof1677;
case 1677:
#line 38874 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1735;
		case 10: goto tr1735;
		case 13: goto tr1735;
		case 39: goto tr2086;
	}
	goto st1677;
tr2083:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1678;
st1678:
	if ( ++( p) == ( pe) )
		goto _test_eof1678;
case 1678:
#line 38890 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2090;
		case 32: goto tr2090;
		case 93: goto tr1926;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1678;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1678;
	} else
		goto st1678;
	goto tr1735;
	}
	_test_eof1679: ( cs) = 1679; goto _test_eof; 
	_test_eof1680: ( cs) = 1680; goto _test_eof; 
	_test_eof1: ( cs) = 1; goto _test_eof; 
	_test_eof1681: ( cs) = 1681; goto _test_eof; 
	_test_eof2: ( cs) = 2; goto _test_eof; 
	_test_eof1682: ( cs) = 1682; goto _test_eof; 
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
	_test_eof1683: ( cs) = 1683; goto _test_eof; 
	_test_eof17: ( cs) = 17; goto _test_eof; 
	_test_eof18: ( cs) = 18; goto _test_eof; 
	_test_eof19: ( cs) = 19; goto _test_eof; 
	_test_eof20: ( cs) = 20; goto _test_eof; 
	_test_eof21: ( cs) = 21; goto _test_eof; 
	_test_eof22: ( cs) = 22; goto _test_eof; 
	_test_eof23: ( cs) = 23; goto _test_eof; 
	_test_eof24: ( cs) = 24; goto _test_eof; 
	_test_eof25: ( cs) = 25; goto _test_eof; 
	_test_eof26: ( cs) = 26; goto _test_eof; 
	_test_eof27: ( cs) = 27; goto _test_eof; 
	_test_eof28: ( cs) = 28; goto _test_eof; 
	_test_eof1684: ( cs) = 1684; goto _test_eof; 
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
	_test_eof45: ( cs) = 45; goto _test_eof; 
	_test_eof46: ( cs) = 46; goto _test_eof; 
	_test_eof1685: ( cs) = 1685; goto _test_eof; 
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
	_test_eof60: ( cs) = 60; goto _test_eof; 
	_test_eof61: ( cs) = 61; goto _test_eof; 
	_test_eof1686: ( cs) = 1686; goto _test_eof; 
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
	_test_eof75: ( cs) = 75; goto _test_eof; 
	_test_eof76: ( cs) = 76; goto _test_eof; 
	_test_eof1687: ( cs) = 1687; goto _test_eof; 
	_test_eof1688: ( cs) = 1688; goto _test_eof; 
	_test_eof77: ( cs) = 77; goto _test_eof; 
	_test_eof1689: ( cs) = 1689; goto _test_eof; 
	_test_eof1690: ( cs) = 1690; goto _test_eof; 
	_test_eof78: ( cs) = 78; goto _test_eof; 
	_test_eof1691: ( cs) = 1691; goto _test_eof; 
	_test_eof79: ( cs) = 79; goto _test_eof; 
	_test_eof80: ( cs) = 80; goto _test_eof; 
	_test_eof81: ( cs) = 81; goto _test_eof; 
	_test_eof1692: ( cs) = 1692; goto _test_eof; 
	_test_eof1693: ( cs) = 1693; goto _test_eof; 
	_test_eof82: ( cs) = 82; goto _test_eof; 
	_test_eof83: ( cs) = 83; goto _test_eof; 
	_test_eof84: ( cs) = 84; goto _test_eof; 
	_test_eof85: ( cs) = 85; goto _test_eof; 
	_test_eof86: ( cs) = 86; goto _test_eof; 
	_test_eof87: ( cs) = 87; goto _test_eof; 
	_test_eof88: ( cs) = 88; goto _test_eof; 
	_test_eof89: ( cs) = 89; goto _test_eof; 
	_test_eof90: ( cs) = 90; goto _test_eof; 
	_test_eof91: ( cs) = 91; goto _test_eof; 
	_test_eof1694: ( cs) = 1694; goto _test_eof; 
	_test_eof92: ( cs) = 92; goto _test_eof; 
	_test_eof93: ( cs) = 93; goto _test_eof; 
	_test_eof94: ( cs) = 94; goto _test_eof; 
	_test_eof95: ( cs) = 95; goto _test_eof; 
	_test_eof96: ( cs) = 96; goto _test_eof; 
	_test_eof97: ( cs) = 97; goto _test_eof; 
	_test_eof98: ( cs) = 98; goto _test_eof; 
	_test_eof99: ( cs) = 99; goto _test_eof; 
	_test_eof100: ( cs) = 100; goto _test_eof; 
	_test_eof101: ( cs) = 101; goto _test_eof; 
	_test_eof1695: ( cs) = 1695; goto _test_eof; 
	_test_eof102: ( cs) = 102; goto _test_eof; 
	_test_eof103: ( cs) = 103; goto _test_eof; 
	_test_eof104: ( cs) = 104; goto _test_eof; 
	_test_eof105: ( cs) = 105; goto _test_eof; 
	_test_eof106: ( cs) = 106; goto _test_eof; 
	_test_eof107: ( cs) = 107; goto _test_eof; 
	_test_eof108: ( cs) = 108; goto _test_eof; 
	_test_eof1696: ( cs) = 1696; goto _test_eof; 
	_test_eof109: ( cs) = 109; goto _test_eof; 
	_test_eof1697: ( cs) = 1697; goto _test_eof; 
	_test_eof110: ( cs) = 110; goto _test_eof; 
	_test_eof111: ( cs) = 111; goto _test_eof; 
	_test_eof112: ( cs) = 112; goto _test_eof; 
	_test_eof113: ( cs) = 113; goto _test_eof; 
	_test_eof114: ( cs) = 114; goto _test_eof; 
	_test_eof115: ( cs) = 115; goto _test_eof; 
	_test_eof116: ( cs) = 116; goto _test_eof; 
	_test_eof117: ( cs) = 117; goto _test_eof; 
	_test_eof1698: ( cs) = 1698; goto _test_eof; 
	_test_eof118: ( cs) = 118; goto _test_eof; 
	_test_eof119: ( cs) = 119; goto _test_eof; 
	_test_eof120: ( cs) = 120; goto _test_eof; 
	_test_eof121: ( cs) = 121; goto _test_eof; 
	_test_eof122: ( cs) = 122; goto _test_eof; 
	_test_eof123: ( cs) = 123; goto _test_eof; 
	_test_eof124: ( cs) = 124; goto _test_eof; 
	_test_eof125: ( cs) = 125; goto _test_eof; 
	_test_eof126: ( cs) = 126; goto _test_eof; 
	_test_eof127: ( cs) = 127; goto _test_eof; 
	_test_eof1699: ( cs) = 1699; goto _test_eof; 
	_test_eof128: ( cs) = 128; goto _test_eof; 
	_test_eof1700: ( cs) = 1700; goto _test_eof; 
	_test_eof129: ( cs) = 129; goto _test_eof; 
	_test_eof130: ( cs) = 130; goto _test_eof; 
	_test_eof131: ( cs) = 131; goto _test_eof; 
	_test_eof132: ( cs) = 132; goto _test_eof; 
	_test_eof133: ( cs) = 133; goto _test_eof; 
	_test_eof134: ( cs) = 134; goto _test_eof; 
	_test_eof1701: ( cs) = 1701; goto _test_eof; 
	_test_eof135: ( cs) = 135; goto _test_eof; 
	_test_eof136: ( cs) = 136; goto _test_eof; 
	_test_eof137: ( cs) = 137; goto _test_eof; 
	_test_eof1702: ( cs) = 1702; goto _test_eof; 
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
	_test_eof174: ( cs) = 174; goto _test_eof; 
	_test_eof175: ( cs) = 175; goto _test_eof; 
	_test_eof1703: ( cs) = 1703; goto _test_eof; 
	_test_eof1704: ( cs) = 1704; goto _test_eof; 
	_test_eof1705: ( cs) = 1705; goto _test_eof; 
	_test_eof1706: ( cs) = 1706; goto _test_eof; 
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
	_test_eof193: ( cs) = 193; goto _test_eof; 
	_test_eof194: ( cs) = 194; goto _test_eof; 
	_test_eof195: ( cs) = 195; goto _test_eof; 
	_test_eof196: ( cs) = 196; goto _test_eof; 
	_test_eof197: ( cs) = 197; goto _test_eof; 
	_test_eof198: ( cs) = 198; goto _test_eof; 
	_test_eof199: ( cs) = 199; goto _test_eof; 
	_test_eof200: ( cs) = 200; goto _test_eof; 
	_test_eof1707: ( cs) = 1707; goto _test_eof; 
	_test_eof201: ( cs) = 201; goto _test_eof; 
	_test_eof202: ( cs) = 202; goto _test_eof; 
	_test_eof203: ( cs) = 203; goto _test_eof; 
	_test_eof204: ( cs) = 204; goto _test_eof; 
	_test_eof205: ( cs) = 205; goto _test_eof; 
	_test_eof206: ( cs) = 206; goto _test_eof; 
	_test_eof207: ( cs) = 207; goto _test_eof; 
	_test_eof208: ( cs) = 208; goto _test_eof; 
	_test_eof209: ( cs) = 209; goto _test_eof; 
	_test_eof210: ( cs) = 210; goto _test_eof; 
	_test_eof211: ( cs) = 211; goto _test_eof; 
	_test_eof212: ( cs) = 212; goto _test_eof; 
	_test_eof213: ( cs) = 213; goto _test_eof; 
	_test_eof214: ( cs) = 214; goto _test_eof; 
	_test_eof215: ( cs) = 215; goto _test_eof; 
	_test_eof1708: ( cs) = 1708; goto _test_eof; 
	_test_eof1709: ( cs) = 1709; goto _test_eof; 
	_test_eof1710: ( cs) = 1710; goto _test_eof; 
	_test_eof216: ( cs) = 216; goto _test_eof; 
	_test_eof217: ( cs) = 217; goto _test_eof; 
	_test_eof218: ( cs) = 218; goto _test_eof; 
	_test_eof1711: ( cs) = 1711; goto _test_eof; 
	_test_eof1712: ( cs) = 1712; goto _test_eof; 
	_test_eof1713: ( cs) = 1713; goto _test_eof; 
	_test_eof219: ( cs) = 219; goto _test_eof; 
	_test_eof1714: ( cs) = 1714; goto _test_eof; 
	_test_eof220: ( cs) = 220; goto _test_eof; 
	_test_eof1715: ( cs) = 1715; goto _test_eof; 
	_test_eof221: ( cs) = 221; goto _test_eof; 
	_test_eof222: ( cs) = 222; goto _test_eof; 
	_test_eof223: ( cs) = 223; goto _test_eof; 
	_test_eof224: ( cs) = 224; goto _test_eof; 
	_test_eof225: ( cs) = 225; goto _test_eof; 
	_test_eof226: ( cs) = 226; goto _test_eof; 
	_test_eof227: ( cs) = 227; goto _test_eof; 
	_test_eof228: ( cs) = 228; goto _test_eof; 
	_test_eof229: ( cs) = 229; goto _test_eof; 
	_test_eof230: ( cs) = 230; goto _test_eof; 
	_test_eof231: ( cs) = 231; goto _test_eof; 
	_test_eof232: ( cs) = 232; goto _test_eof; 
	_test_eof233: ( cs) = 233; goto _test_eof; 
	_test_eof1716: ( cs) = 1716; goto _test_eof; 
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
	_test_eof1717: ( cs) = 1717; goto _test_eof; 
	_test_eof246: ( cs) = 246; goto _test_eof; 
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
	_test_eof1718: ( cs) = 1718; goto _test_eof; 
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
	_test_eof292: ( cs) = 292; goto _test_eof; 
	_test_eof293: ( cs) = 293; goto _test_eof; 
	_test_eof294: ( cs) = 294; goto _test_eof; 
	_test_eof295: ( cs) = 295; goto _test_eof; 
	_test_eof296: ( cs) = 296; goto _test_eof; 
	_test_eof297: ( cs) = 297; goto _test_eof; 
	_test_eof1719: ( cs) = 1719; goto _test_eof; 
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
	_test_eof1720: ( cs) = 1720; goto _test_eof; 
	_test_eof1721: ( cs) = 1721; goto _test_eof; 
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
	_test_eof1722: ( cs) = 1722; goto _test_eof; 
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
	_test_eof383: ( cs) = 383; goto _test_eof; 
	_test_eof384: ( cs) = 384; goto _test_eof; 
	_test_eof385: ( cs) = 385; goto _test_eof; 
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
	_test_eof397: ( cs) = 397; goto _test_eof; 
	_test_eof398: ( cs) = 398; goto _test_eof; 
	_test_eof399: ( cs) = 399; goto _test_eof; 
	_test_eof400: ( cs) = 400; goto _test_eof; 
	_test_eof1723: ( cs) = 1723; goto _test_eof; 
	_test_eof401: ( cs) = 401; goto _test_eof; 
	_test_eof402: ( cs) = 402; goto _test_eof; 
	_test_eof403: ( cs) = 403; goto _test_eof; 
	_test_eof1724: ( cs) = 1724; goto _test_eof; 
	_test_eof404: ( cs) = 404; goto _test_eof; 
	_test_eof405: ( cs) = 405; goto _test_eof; 
	_test_eof406: ( cs) = 406; goto _test_eof; 
	_test_eof407: ( cs) = 407; goto _test_eof; 
	_test_eof408: ( cs) = 408; goto _test_eof; 
	_test_eof409: ( cs) = 409; goto _test_eof; 
	_test_eof410: ( cs) = 410; goto _test_eof; 
	_test_eof411: ( cs) = 411; goto _test_eof; 
	_test_eof412: ( cs) = 412; goto _test_eof; 
	_test_eof413: ( cs) = 413; goto _test_eof; 
	_test_eof414: ( cs) = 414; goto _test_eof; 
	_test_eof1725: ( cs) = 1725; goto _test_eof; 
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
	_test_eof1726: ( cs) = 1726; goto _test_eof; 
	_test_eof428: ( cs) = 428; goto _test_eof; 
	_test_eof429: ( cs) = 429; goto _test_eof; 
	_test_eof430: ( cs) = 430; goto _test_eof; 
	_test_eof431: ( cs) = 431; goto _test_eof; 
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
	_test_eof443: ( cs) = 443; goto _test_eof; 
	_test_eof444: ( cs) = 444; goto _test_eof; 
	_test_eof445: ( cs) = 445; goto _test_eof; 
	_test_eof446: ( cs) = 446; goto _test_eof; 
	_test_eof447: ( cs) = 447; goto _test_eof; 
	_test_eof448: ( cs) = 448; goto _test_eof; 
	_test_eof449: ( cs) = 449; goto _test_eof; 
	_test_eof1727: ( cs) = 1727; goto _test_eof; 
	_test_eof450: ( cs) = 450; goto _test_eof; 
	_test_eof451: ( cs) = 451; goto _test_eof; 
	_test_eof452: ( cs) = 452; goto _test_eof; 
	_test_eof453: ( cs) = 453; goto _test_eof; 
	_test_eof454: ( cs) = 454; goto _test_eof; 
	_test_eof455: ( cs) = 455; goto _test_eof; 
	_test_eof456: ( cs) = 456; goto _test_eof; 
	_test_eof457: ( cs) = 457; goto _test_eof; 
	_test_eof458: ( cs) = 458; goto _test_eof; 
	_test_eof459: ( cs) = 459; goto _test_eof; 
	_test_eof460: ( cs) = 460; goto _test_eof; 
	_test_eof1728: ( cs) = 1728; goto _test_eof; 
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
	_test_eof1729: ( cs) = 1729; goto _test_eof; 
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
	_test_eof501: ( cs) = 501; goto _test_eof; 
	_test_eof502: ( cs) = 502; goto _test_eof; 
	_test_eof503: ( cs) = 503; goto _test_eof; 
	_test_eof504: ( cs) = 504; goto _test_eof; 
	_test_eof505: ( cs) = 505; goto _test_eof; 
	_test_eof506: ( cs) = 506; goto _test_eof; 
	_test_eof507: ( cs) = 507; goto _test_eof; 
	_test_eof508: ( cs) = 508; goto _test_eof; 
	_test_eof509: ( cs) = 509; goto _test_eof; 
	_test_eof510: ( cs) = 510; goto _test_eof; 
	_test_eof511: ( cs) = 511; goto _test_eof; 
	_test_eof512: ( cs) = 512; goto _test_eof; 
	_test_eof513: ( cs) = 513; goto _test_eof; 
	_test_eof514: ( cs) = 514; goto _test_eof; 
	_test_eof515: ( cs) = 515; goto _test_eof; 
	_test_eof516: ( cs) = 516; goto _test_eof; 
	_test_eof517: ( cs) = 517; goto _test_eof; 
	_test_eof518: ( cs) = 518; goto _test_eof; 
	_test_eof1730: ( cs) = 1730; goto _test_eof; 
	_test_eof519: ( cs) = 519; goto _test_eof; 
	_test_eof520: ( cs) = 520; goto _test_eof; 
	_test_eof521: ( cs) = 521; goto _test_eof; 
	_test_eof522: ( cs) = 522; goto _test_eof; 
	_test_eof523: ( cs) = 523; goto _test_eof; 
	_test_eof524: ( cs) = 524; goto _test_eof; 
	_test_eof525: ( cs) = 525; goto _test_eof; 
	_test_eof526: ( cs) = 526; goto _test_eof; 
	_test_eof527: ( cs) = 527; goto _test_eof; 
	_test_eof1731: ( cs) = 1731; goto _test_eof; 
	_test_eof1732: ( cs) = 1732; goto _test_eof; 
	_test_eof528: ( cs) = 528; goto _test_eof; 
	_test_eof529: ( cs) = 529; goto _test_eof; 
	_test_eof530: ( cs) = 530; goto _test_eof; 
	_test_eof531: ( cs) = 531; goto _test_eof; 
	_test_eof1733: ( cs) = 1733; goto _test_eof; 
	_test_eof1734: ( cs) = 1734; goto _test_eof; 
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
	_test_eof1735: ( cs) = 1735; goto _test_eof; 
	_test_eof1736: ( cs) = 1736; goto _test_eof; 
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
	_test_eof566: ( cs) = 566; goto _test_eof; 
	_test_eof567: ( cs) = 567; goto _test_eof; 
	_test_eof568: ( cs) = 568; goto _test_eof; 
	_test_eof569: ( cs) = 569; goto _test_eof; 
	_test_eof570: ( cs) = 570; goto _test_eof; 
	_test_eof571: ( cs) = 571; goto _test_eof; 
	_test_eof572: ( cs) = 572; goto _test_eof; 
	_test_eof573: ( cs) = 573; goto _test_eof; 
	_test_eof574: ( cs) = 574; goto _test_eof; 
	_test_eof575: ( cs) = 575; goto _test_eof; 
	_test_eof1737: ( cs) = 1737; goto _test_eof; 
	_test_eof1738: ( cs) = 1738; goto _test_eof; 
	_test_eof576: ( cs) = 576; goto _test_eof; 
	_test_eof577: ( cs) = 577; goto _test_eof; 
	_test_eof578: ( cs) = 578; goto _test_eof; 
	_test_eof579: ( cs) = 579; goto _test_eof; 
	_test_eof1739: ( cs) = 1739; goto _test_eof; 
	_test_eof1740: ( cs) = 1740; goto _test_eof; 
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
	_test_eof1741: ( cs) = 1741; goto _test_eof; 
	_test_eof1742: ( cs) = 1742; goto _test_eof; 
	_test_eof592: ( cs) = 592; goto _test_eof; 
	_test_eof1743: ( cs) = 1743; goto _test_eof; 
	_test_eof1744: ( cs) = 1744; goto _test_eof; 
	_test_eof593: ( cs) = 593; goto _test_eof; 
	_test_eof594: ( cs) = 594; goto _test_eof; 
	_test_eof595: ( cs) = 595; goto _test_eof; 
	_test_eof596: ( cs) = 596; goto _test_eof; 
	_test_eof597: ( cs) = 597; goto _test_eof; 
	_test_eof598: ( cs) = 598; goto _test_eof; 
	_test_eof599: ( cs) = 599; goto _test_eof; 
	_test_eof600: ( cs) = 600; goto _test_eof; 
	_test_eof1745: ( cs) = 1745; goto _test_eof; 
	_test_eof1746: ( cs) = 1746; goto _test_eof; 
	_test_eof601: ( cs) = 601; goto _test_eof; 
	_test_eof1747: ( cs) = 1747; goto _test_eof; 
	_test_eof602: ( cs) = 602; goto _test_eof; 
	_test_eof603: ( cs) = 603; goto _test_eof; 
	_test_eof604: ( cs) = 604; goto _test_eof; 
	_test_eof605: ( cs) = 605; goto _test_eof; 
	_test_eof606: ( cs) = 606; goto _test_eof; 
	_test_eof607: ( cs) = 607; goto _test_eof; 
	_test_eof608: ( cs) = 608; goto _test_eof; 
	_test_eof609: ( cs) = 609; goto _test_eof; 
	_test_eof610: ( cs) = 610; goto _test_eof; 
	_test_eof611: ( cs) = 611; goto _test_eof; 
	_test_eof612: ( cs) = 612; goto _test_eof; 
	_test_eof613: ( cs) = 613; goto _test_eof; 
	_test_eof614: ( cs) = 614; goto _test_eof; 
	_test_eof615: ( cs) = 615; goto _test_eof; 
	_test_eof616: ( cs) = 616; goto _test_eof; 
	_test_eof617: ( cs) = 617; goto _test_eof; 
	_test_eof618: ( cs) = 618; goto _test_eof; 
	_test_eof619: ( cs) = 619; goto _test_eof; 
	_test_eof1748: ( cs) = 1748; goto _test_eof; 
	_test_eof620: ( cs) = 620; goto _test_eof; 
	_test_eof621: ( cs) = 621; goto _test_eof; 
	_test_eof622: ( cs) = 622; goto _test_eof; 
	_test_eof623: ( cs) = 623; goto _test_eof; 
	_test_eof624: ( cs) = 624; goto _test_eof; 
	_test_eof625: ( cs) = 625; goto _test_eof; 
	_test_eof626: ( cs) = 626; goto _test_eof; 
	_test_eof627: ( cs) = 627; goto _test_eof; 
	_test_eof628: ( cs) = 628; goto _test_eof; 
	_test_eof1749: ( cs) = 1749; goto _test_eof; 
	_test_eof1750: ( cs) = 1750; goto _test_eof; 
	_test_eof1751: ( cs) = 1751; goto _test_eof; 
	_test_eof1752: ( cs) = 1752; goto _test_eof; 
	_test_eof1753: ( cs) = 1753; goto _test_eof; 
	_test_eof629: ( cs) = 629; goto _test_eof; 
	_test_eof630: ( cs) = 630; goto _test_eof; 
	_test_eof1754: ( cs) = 1754; goto _test_eof; 
	_test_eof1755: ( cs) = 1755; goto _test_eof; 
	_test_eof1756: ( cs) = 1756; goto _test_eof; 
	_test_eof1757: ( cs) = 1757; goto _test_eof; 
	_test_eof1758: ( cs) = 1758; goto _test_eof; 
	_test_eof1759: ( cs) = 1759; goto _test_eof; 
	_test_eof631: ( cs) = 631; goto _test_eof; 
	_test_eof632: ( cs) = 632; goto _test_eof; 
	_test_eof1760: ( cs) = 1760; goto _test_eof; 
	_test_eof633: ( cs) = 633; goto _test_eof; 
	_test_eof634: ( cs) = 634; goto _test_eof; 
	_test_eof635: ( cs) = 635; goto _test_eof; 
	_test_eof636: ( cs) = 636; goto _test_eof; 
	_test_eof637: ( cs) = 637; goto _test_eof; 
	_test_eof638: ( cs) = 638; goto _test_eof; 
	_test_eof639: ( cs) = 639; goto _test_eof; 
	_test_eof640: ( cs) = 640; goto _test_eof; 
	_test_eof641: ( cs) = 641; goto _test_eof; 
	_test_eof1761: ( cs) = 1761; goto _test_eof; 
	_test_eof1762: ( cs) = 1762; goto _test_eof; 
	_test_eof1763: ( cs) = 1763; goto _test_eof; 
	_test_eof1764: ( cs) = 1764; goto _test_eof; 
	_test_eof1765: ( cs) = 1765; goto _test_eof; 
	_test_eof642: ( cs) = 642; goto _test_eof; 
	_test_eof643: ( cs) = 643; goto _test_eof; 
	_test_eof644: ( cs) = 644; goto _test_eof; 
	_test_eof645: ( cs) = 645; goto _test_eof; 
	_test_eof646: ( cs) = 646; goto _test_eof; 
	_test_eof647: ( cs) = 647; goto _test_eof; 
	_test_eof648: ( cs) = 648; goto _test_eof; 
	_test_eof649: ( cs) = 649; goto _test_eof; 
	_test_eof650: ( cs) = 650; goto _test_eof; 
	_test_eof651: ( cs) = 651; goto _test_eof; 
	_test_eof1766: ( cs) = 1766; goto _test_eof; 
	_test_eof1767: ( cs) = 1767; goto _test_eof; 
	_test_eof1768: ( cs) = 1768; goto _test_eof; 
	_test_eof1769: ( cs) = 1769; goto _test_eof; 
	_test_eof652: ( cs) = 652; goto _test_eof; 
	_test_eof653: ( cs) = 653; goto _test_eof; 
	_test_eof1770: ( cs) = 1770; goto _test_eof; 
	_test_eof1771: ( cs) = 1771; goto _test_eof; 
	_test_eof1772: ( cs) = 1772; goto _test_eof; 
	_test_eof654: ( cs) = 654; goto _test_eof; 
	_test_eof655: ( cs) = 655; goto _test_eof; 
	_test_eof1773: ( cs) = 1773; goto _test_eof; 
	_test_eof1774: ( cs) = 1774; goto _test_eof; 
	_test_eof1775: ( cs) = 1775; goto _test_eof; 
	_test_eof1776: ( cs) = 1776; goto _test_eof; 
	_test_eof1777: ( cs) = 1777; goto _test_eof; 
	_test_eof1778: ( cs) = 1778; goto _test_eof; 
	_test_eof1779: ( cs) = 1779; goto _test_eof; 
	_test_eof1780: ( cs) = 1780; goto _test_eof; 
	_test_eof656: ( cs) = 656; goto _test_eof; 
	_test_eof657: ( cs) = 657; goto _test_eof; 
	_test_eof1781: ( cs) = 1781; goto _test_eof; 
	_test_eof1782: ( cs) = 1782; goto _test_eof; 
	_test_eof1783: ( cs) = 1783; goto _test_eof; 
	_test_eof658: ( cs) = 658; goto _test_eof; 
	_test_eof659: ( cs) = 659; goto _test_eof; 
	_test_eof1784: ( cs) = 1784; goto _test_eof; 
	_test_eof1785: ( cs) = 1785; goto _test_eof; 
	_test_eof1786: ( cs) = 1786; goto _test_eof; 
	_test_eof1787: ( cs) = 1787; goto _test_eof; 
	_test_eof1788: ( cs) = 1788; goto _test_eof; 
	_test_eof1789: ( cs) = 1789; goto _test_eof; 
	_test_eof660: ( cs) = 660; goto _test_eof; 
	_test_eof661: ( cs) = 661; goto _test_eof; 
	_test_eof1790: ( cs) = 1790; goto _test_eof; 
	_test_eof662: ( cs) = 662; goto _test_eof; 
	_test_eof1791: ( cs) = 1791; goto _test_eof; 
	_test_eof1792: ( cs) = 1792; goto _test_eof; 
	_test_eof1793: ( cs) = 1793; goto _test_eof; 
	_test_eof663: ( cs) = 663; goto _test_eof; 
	_test_eof664: ( cs) = 664; goto _test_eof; 
	_test_eof1794: ( cs) = 1794; goto _test_eof; 
	_test_eof1795: ( cs) = 1795; goto _test_eof; 
	_test_eof1796: ( cs) = 1796; goto _test_eof; 
	_test_eof1797: ( cs) = 1797; goto _test_eof; 
	_test_eof1798: ( cs) = 1798; goto _test_eof; 
	_test_eof665: ( cs) = 665; goto _test_eof; 
	_test_eof666: ( cs) = 666; goto _test_eof; 
	_test_eof1799: ( cs) = 1799; goto _test_eof; 
	_test_eof1800: ( cs) = 1800; goto _test_eof; 
	_test_eof1801: ( cs) = 1801; goto _test_eof; 
	_test_eof1802: ( cs) = 1802; goto _test_eof; 
	_test_eof1803: ( cs) = 1803; goto _test_eof; 
	_test_eof667: ( cs) = 667; goto _test_eof; 
	_test_eof668: ( cs) = 668; goto _test_eof; 
	_test_eof669: ( cs) = 669; goto _test_eof; 
	_test_eof1804: ( cs) = 1804; goto _test_eof; 
	_test_eof670: ( cs) = 670; goto _test_eof; 
	_test_eof671: ( cs) = 671; goto _test_eof; 
	_test_eof672: ( cs) = 672; goto _test_eof; 
	_test_eof673: ( cs) = 673; goto _test_eof; 
	_test_eof674: ( cs) = 674; goto _test_eof; 
	_test_eof675: ( cs) = 675; goto _test_eof; 
	_test_eof676: ( cs) = 676; goto _test_eof; 
	_test_eof677: ( cs) = 677; goto _test_eof; 
	_test_eof678: ( cs) = 678; goto _test_eof; 
	_test_eof679: ( cs) = 679; goto _test_eof; 
	_test_eof680: ( cs) = 680; goto _test_eof; 
	_test_eof1805: ( cs) = 1805; goto _test_eof; 
	_test_eof681: ( cs) = 681; goto _test_eof; 
	_test_eof682: ( cs) = 682; goto _test_eof; 
	_test_eof1806: ( cs) = 1806; goto _test_eof; 
	_test_eof1807: ( cs) = 1807; goto _test_eof; 
	_test_eof1808: ( cs) = 1808; goto _test_eof; 
	_test_eof1809: ( cs) = 1809; goto _test_eof; 
	_test_eof1810: ( cs) = 1810; goto _test_eof; 
	_test_eof1811: ( cs) = 1811; goto _test_eof; 
	_test_eof1812: ( cs) = 1812; goto _test_eof; 
	_test_eof1813: ( cs) = 1813; goto _test_eof; 
	_test_eof1814: ( cs) = 1814; goto _test_eof; 
	_test_eof683: ( cs) = 683; goto _test_eof; 
	_test_eof684: ( cs) = 684; goto _test_eof; 
	_test_eof685: ( cs) = 685; goto _test_eof; 
	_test_eof686: ( cs) = 686; goto _test_eof; 
	_test_eof687: ( cs) = 687; goto _test_eof; 
	_test_eof688: ( cs) = 688; goto _test_eof; 
	_test_eof689: ( cs) = 689; goto _test_eof; 
	_test_eof690: ( cs) = 690; goto _test_eof; 
	_test_eof691: ( cs) = 691; goto _test_eof; 
	_test_eof1815: ( cs) = 1815; goto _test_eof; 
	_test_eof692: ( cs) = 692; goto _test_eof; 
	_test_eof693: ( cs) = 693; goto _test_eof; 
	_test_eof694: ( cs) = 694; goto _test_eof; 
	_test_eof695: ( cs) = 695; goto _test_eof; 
	_test_eof696: ( cs) = 696; goto _test_eof; 
	_test_eof697: ( cs) = 697; goto _test_eof; 
	_test_eof698: ( cs) = 698; goto _test_eof; 
	_test_eof699: ( cs) = 699; goto _test_eof; 
	_test_eof700: ( cs) = 700; goto _test_eof; 
	_test_eof701: ( cs) = 701; goto _test_eof; 
	_test_eof1816: ( cs) = 1816; goto _test_eof; 
	_test_eof702: ( cs) = 702; goto _test_eof; 
	_test_eof703: ( cs) = 703; goto _test_eof; 
	_test_eof704: ( cs) = 704; goto _test_eof; 
	_test_eof705: ( cs) = 705; goto _test_eof; 
	_test_eof706: ( cs) = 706; goto _test_eof; 
	_test_eof707: ( cs) = 707; goto _test_eof; 
	_test_eof708: ( cs) = 708; goto _test_eof; 
	_test_eof709: ( cs) = 709; goto _test_eof; 
	_test_eof710: ( cs) = 710; goto _test_eof; 
	_test_eof711: ( cs) = 711; goto _test_eof; 
	_test_eof712: ( cs) = 712; goto _test_eof; 
	_test_eof1817: ( cs) = 1817; goto _test_eof; 
	_test_eof713: ( cs) = 713; goto _test_eof; 
	_test_eof714: ( cs) = 714; goto _test_eof; 
	_test_eof715: ( cs) = 715; goto _test_eof; 
	_test_eof716: ( cs) = 716; goto _test_eof; 
	_test_eof717: ( cs) = 717; goto _test_eof; 
	_test_eof718: ( cs) = 718; goto _test_eof; 
	_test_eof719: ( cs) = 719; goto _test_eof; 
	_test_eof720: ( cs) = 720; goto _test_eof; 
	_test_eof721: ( cs) = 721; goto _test_eof; 
	_test_eof722: ( cs) = 722; goto _test_eof; 
	_test_eof723: ( cs) = 723; goto _test_eof; 
	_test_eof724: ( cs) = 724; goto _test_eof; 
	_test_eof725: ( cs) = 725; goto _test_eof; 
	_test_eof1818: ( cs) = 1818; goto _test_eof; 
	_test_eof726: ( cs) = 726; goto _test_eof; 
	_test_eof727: ( cs) = 727; goto _test_eof; 
	_test_eof728: ( cs) = 728; goto _test_eof; 
	_test_eof729: ( cs) = 729; goto _test_eof; 
	_test_eof730: ( cs) = 730; goto _test_eof; 
	_test_eof731: ( cs) = 731; goto _test_eof; 
	_test_eof732: ( cs) = 732; goto _test_eof; 
	_test_eof733: ( cs) = 733; goto _test_eof; 
	_test_eof734: ( cs) = 734; goto _test_eof; 
	_test_eof735: ( cs) = 735; goto _test_eof; 
	_test_eof1819: ( cs) = 1819; goto _test_eof; 
	_test_eof1820: ( cs) = 1820; goto _test_eof; 
	_test_eof1821: ( cs) = 1821; goto _test_eof; 
	_test_eof1822: ( cs) = 1822; goto _test_eof; 
	_test_eof1823: ( cs) = 1823; goto _test_eof; 
	_test_eof1824: ( cs) = 1824; goto _test_eof; 
	_test_eof1825: ( cs) = 1825; goto _test_eof; 
	_test_eof1826: ( cs) = 1826; goto _test_eof; 
	_test_eof1827: ( cs) = 1827; goto _test_eof; 
	_test_eof1828: ( cs) = 1828; goto _test_eof; 
	_test_eof1829: ( cs) = 1829; goto _test_eof; 
	_test_eof1830: ( cs) = 1830; goto _test_eof; 
	_test_eof1831: ( cs) = 1831; goto _test_eof; 
	_test_eof736: ( cs) = 736; goto _test_eof; 
	_test_eof737: ( cs) = 737; goto _test_eof; 
	_test_eof1832: ( cs) = 1832; goto _test_eof; 
	_test_eof1833: ( cs) = 1833; goto _test_eof; 
	_test_eof1834: ( cs) = 1834; goto _test_eof; 
	_test_eof1835: ( cs) = 1835; goto _test_eof; 
	_test_eof1836: ( cs) = 1836; goto _test_eof; 
	_test_eof738: ( cs) = 738; goto _test_eof; 
	_test_eof739: ( cs) = 739; goto _test_eof; 
	_test_eof1837: ( cs) = 1837; goto _test_eof; 
	_test_eof1838: ( cs) = 1838; goto _test_eof; 
	_test_eof1839: ( cs) = 1839; goto _test_eof; 
	_test_eof1840: ( cs) = 1840; goto _test_eof; 
	_test_eof740: ( cs) = 740; goto _test_eof; 
	_test_eof741: ( cs) = 741; goto _test_eof; 
	_test_eof742: ( cs) = 742; goto _test_eof; 
	_test_eof743: ( cs) = 743; goto _test_eof; 
	_test_eof744: ( cs) = 744; goto _test_eof; 
	_test_eof745: ( cs) = 745; goto _test_eof; 
	_test_eof746: ( cs) = 746; goto _test_eof; 
	_test_eof747: ( cs) = 747; goto _test_eof; 
	_test_eof748: ( cs) = 748; goto _test_eof; 
	_test_eof1841: ( cs) = 1841; goto _test_eof; 
	_test_eof1842: ( cs) = 1842; goto _test_eof; 
	_test_eof1843: ( cs) = 1843; goto _test_eof; 
	_test_eof1844: ( cs) = 1844; goto _test_eof; 
	_test_eof1845: ( cs) = 1845; goto _test_eof; 
	_test_eof749: ( cs) = 749; goto _test_eof; 
	_test_eof750: ( cs) = 750; goto _test_eof; 
	_test_eof1846: ( cs) = 1846; goto _test_eof; 
	_test_eof1847: ( cs) = 1847; goto _test_eof; 
	_test_eof1848: ( cs) = 1848; goto _test_eof; 
	_test_eof1849: ( cs) = 1849; goto _test_eof; 
	_test_eof1850: ( cs) = 1850; goto _test_eof; 
	_test_eof751: ( cs) = 751; goto _test_eof; 
	_test_eof752: ( cs) = 752; goto _test_eof; 
	_test_eof1851: ( cs) = 1851; goto _test_eof; 
	_test_eof1852: ( cs) = 1852; goto _test_eof; 
	_test_eof1853: ( cs) = 1853; goto _test_eof; 
	_test_eof753: ( cs) = 753; goto _test_eof; 
	_test_eof754: ( cs) = 754; goto _test_eof; 
	_test_eof1854: ( cs) = 1854; goto _test_eof; 
	_test_eof755: ( cs) = 755; goto _test_eof; 
	_test_eof756: ( cs) = 756; goto _test_eof; 
	_test_eof757: ( cs) = 757; goto _test_eof; 
	_test_eof758: ( cs) = 758; goto _test_eof; 
	_test_eof759: ( cs) = 759; goto _test_eof; 
	_test_eof760: ( cs) = 760; goto _test_eof; 
	_test_eof761: ( cs) = 761; goto _test_eof; 
	_test_eof762: ( cs) = 762; goto _test_eof; 
	_test_eof763: ( cs) = 763; goto _test_eof; 
	_test_eof1855: ( cs) = 1855; goto _test_eof; 
	_test_eof1856: ( cs) = 1856; goto _test_eof; 
	_test_eof1857: ( cs) = 1857; goto _test_eof; 
	_test_eof1858: ( cs) = 1858; goto _test_eof; 
	_test_eof764: ( cs) = 764; goto _test_eof; 
	_test_eof765: ( cs) = 765; goto _test_eof; 
	_test_eof1859: ( cs) = 1859; goto _test_eof; 
	_test_eof1860: ( cs) = 1860; goto _test_eof; 
	_test_eof1861: ( cs) = 1861; goto _test_eof; 
	_test_eof1862: ( cs) = 1862; goto _test_eof; 
	_test_eof1863: ( cs) = 1863; goto _test_eof; 
	_test_eof1864: ( cs) = 1864; goto _test_eof; 
	_test_eof1865: ( cs) = 1865; goto _test_eof; 
	_test_eof766: ( cs) = 766; goto _test_eof; 
	_test_eof767: ( cs) = 767; goto _test_eof; 
	_test_eof1866: ( cs) = 1866; goto _test_eof; 
	_test_eof1867: ( cs) = 1867; goto _test_eof; 
	_test_eof1868: ( cs) = 1868; goto _test_eof; 
	_test_eof1869: ( cs) = 1869; goto _test_eof; 
	_test_eof768: ( cs) = 768; goto _test_eof; 
	_test_eof769: ( cs) = 769; goto _test_eof; 
	_test_eof1870: ( cs) = 1870; goto _test_eof; 
	_test_eof1871: ( cs) = 1871; goto _test_eof; 
	_test_eof1872: ( cs) = 1872; goto _test_eof; 
	_test_eof1873: ( cs) = 1873; goto _test_eof; 
	_test_eof1874: ( cs) = 1874; goto _test_eof; 
	_test_eof770: ( cs) = 770; goto _test_eof; 
	_test_eof771: ( cs) = 771; goto _test_eof; 
	_test_eof772: ( cs) = 772; goto _test_eof; 
	_test_eof773: ( cs) = 773; goto _test_eof; 
	_test_eof774: ( cs) = 774; goto _test_eof; 
	_test_eof775: ( cs) = 775; goto _test_eof; 
	_test_eof776: ( cs) = 776; goto _test_eof; 
	_test_eof1875: ( cs) = 1875; goto _test_eof; 
	_test_eof777: ( cs) = 777; goto _test_eof; 
	_test_eof778: ( cs) = 778; goto _test_eof; 
	_test_eof779: ( cs) = 779; goto _test_eof; 
	_test_eof780: ( cs) = 780; goto _test_eof; 
	_test_eof781: ( cs) = 781; goto _test_eof; 
	_test_eof782: ( cs) = 782; goto _test_eof; 
	_test_eof783: ( cs) = 783; goto _test_eof; 
	_test_eof784: ( cs) = 784; goto _test_eof; 
	_test_eof1876: ( cs) = 1876; goto _test_eof; 
	_test_eof1877: ( cs) = 1877; goto _test_eof; 
	_test_eof1878: ( cs) = 1878; goto _test_eof; 
	_test_eof1879: ( cs) = 1879; goto _test_eof; 
	_test_eof1880: ( cs) = 1880; goto _test_eof; 
	_test_eof1881: ( cs) = 1881; goto _test_eof; 
	_test_eof1882: ( cs) = 1882; goto _test_eof; 
	_test_eof1883: ( cs) = 1883; goto _test_eof; 
	_test_eof785: ( cs) = 785; goto _test_eof; 
	_test_eof786: ( cs) = 786; goto _test_eof; 
	_test_eof1884: ( cs) = 1884; goto _test_eof; 
	_test_eof1885: ( cs) = 1885; goto _test_eof; 
	_test_eof1886: ( cs) = 1886; goto _test_eof; 
	_test_eof1887: ( cs) = 1887; goto _test_eof; 
	_test_eof1888: ( cs) = 1888; goto _test_eof; 
	_test_eof1889: ( cs) = 1889; goto _test_eof; 
	_test_eof787: ( cs) = 787; goto _test_eof; 
	_test_eof788: ( cs) = 788; goto _test_eof; 
	_test_eof1890: ( cs) = 1890; goto _test_eof; 
	_test_eof1891: ( cs) = 1891; goto _test_eof; 
	_test_eof1892: ( cs) = 1892; goto _test_eof; 
	_test_eof1893: ( cs) = 1893; goto _test_eof; 
	_test_eof1894: ( cs) = 1894; goto _test_eof; 
	_test_eof1895: ( cs) = 1895; goto _test_eof; 
	_test_eof1896: ( cs) = 1896; goto _test_eof; 
	_test_eof1897: ( cs) = 1897; goto _test_eof; 
	_test_eof1898: ( cs) = 1898; goto _test_eof; 
	_test_eof789: ( cs) = 789; goto _test_eof; 
	_test_eof790: ( cs) = 790; goto _test_eof; 
	_test_eof1899: ( cs) = 1899; goto _test_eof; 
	_test_eof1900: ( cs) = 1900; goto _test_eof; 
	_test_eof1901: ( cs) = 1901; goto _test_eof; 
	_test_eof1902: ( cs) = 1902; goto _test_eof; 
	_test_eof1903: ( cs) = 1903; goto _test_eof; 
	_test_eof791: ( cs) = 791; goto _test_eof; 
	_test_eof792: ( cs) = 792; goto _test_eof; 
	_test_eof793: ( cs) = 793; goto _test_eof; 
	_test_eof1904: ( cs) = 1904; goto _test_eof; 
	_test_eof794: ( cs) = 794; goto _test_eof; 
	_test_eof795: ( cs) = 795; goto _test_eof; 
	_test_eof796: ( cs) = 796; goto _test_eof; 
	_test_eof797: ( cs) = 797; goto _test_eof; 
	_test_eof798: ( cs) = 798; goto _test_eof; 
	_test_eof799: ( cs) = 799; goto _test_eof; 
	_test_eof800: ( cs) = 800; goto _test_eof; 
	_test_eof801: ( cs) = 801; goto _test_eof; 
	_test_eof802: ( cs) = 802; goto _test_eof; 
	_test_eof1905: ( cs) = 1905; goto _test_eof; 
	_test_eof803: ( cs) = 803; goto _test_eof; 
	_test_eof804: ( cs) = 804; goto _test_eof; 
	_test_eof805: ( cs) = 805; goto _test_eof; 
	_test_eof806: ( cs) = 806; goto _test_eof; 
	_test_eof807: ( cs) = 807; goto _test_eof; 
	_test_eof808: ( cs) = 808; goto _test_eof; 
	_test_eof1906: ( cs) = 1906; goto _test_eof; 
	_test_eof1907: ( cs) = 1907; goto _test_eof; 
	_test_eof1908: ( cs) = 1908; goto _test_eof; 
	_test_eof1909: ( cs) = 1909; goto _test_eof; 
	_test_eof1910: ( cs) = 1910; goto _test_eof; 
	_test_eof1911: ( cs) = 1911; goto _test_eof; 
	_test_eof1912: ( cs) = 1912; goto _test_eof; 
	_test_eof1913: ( cs) = 1913; goto _test_eof; 
	_test_eof1914: ( cs) = 1914; goto _test_eof; 
	_test_eof1915: ( cs) = 1915; goto _test_eof; 
	_test_eof1916: ( cs) = 1916; goto _test_eof; 
	_test_eof1917: ( cs) = 1917; goto _test_eof; 
	_test_eof1918: ( cs) = 1918; goto _test_eof; 
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
	_test_eof1919: ( cs) = 1919; goto _test_eof; 
	_test_eof834: ( cs) = 834; goto _test_eof; 
	_test_eof835: ( cs) = 835; goto _test_eof; 
	_test_eof1920: ( cs) = 1920; goto _test_eof; 
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
	_test_eof870: ( cs) = 870; goto _test_eof; 
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
	_test_eof1921: ( cs) = 1921; goto _test_eof; 
	_test_eof901: ( cs) = 901; goto _test_eof; 
	_test_eof1922: ( cs) = 1922; goto _test_eof; 
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
	_test_eof1923: ( cs) = 1923; goto _test_eof; 
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
	_test_eof1049: ( cs) = 1049; goto _test_eof; 
	_test_eof1050: ( cs) = 1050; goto _test_eof; 
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
	_test_eof1068: ( cs) = 1068; goto _test_eof; 
	_test_eof1069: ( cs) = 1069; goto _test_eof; 
	_test_eof1070: ( cs) = 1070; goto _test_eof; 
	_test_eof1071: ( cs) = 1071; goto _test_eof; 
	_test_eof1072: ( cs) = 1072; goto _test_eof; 
	_test_eof1073: ( cs) = 1073; goto _test_eof; 
	_test_eof1074: ( cs) = 1074; goto _test_eof; 
	_test_eof1924: ( cs) = 1924; goto _test_eof; 
	_test_eof1075: ( cs) = 1075; goto _test_eof; 
	_test_eof1076: ( cs) = 1076; goto _test_eof; 
	_test_eof1925: ( cs) = 1925; goto _test_eof; 
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
	_test_eof1926: ( cs) = 1926; goto _test_eof; 
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
	_test_eof1927: ( cs) = 1927; goto _test_eof; 
	_test_eof1146: ( cs) = 1146; goto _test_eof; 
	_test_eof1147: ( cs) = 1147; goto _test_eof; 
	_test_eof1148: ( cs) = 1148; goto _test_eof; 
	_test_eof1149: ( cs) = 1149; goto _test_eof; 
	_test_eof1150: ( cs) = 1150; goto _test_eof; 
	_test_eof1151: ( cs) = 1151; goto _test_eof; 
	_test_eof1152: ( cs) = 1152; goto _test_eof; 
	_test_eof1153: ( cs) = 1153; goto _test_eof; 
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
	_test_eof1928: ( cs) = 1928; goto _test_eof; 
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
	_test_eof1181: ( cs) = 1181; goto _test_eof; 
	_test_eof1182: ( cs) = 1182; goto _test_eof; 
	_test_eof1183: ( cs) = 1183; goto _test_eof; 
	_test_eof1184: ( cs) = 1184; goto _test_eof; 
	_test_eof1185: ( cs) = 1185; goto _test_eof; 
	_test_eof1186: ( cs) = 1186; goto _test_eof; 
	_test_eof1187: ( cs) = 1187; goto _test_eof; 
	_test_eof1188: ( cs) = 1188; goto _test_eof; 
	_test_eof1929: ( cs) = 1929; goto _test_eof; 
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
	_test_eof1930: ( cs) = 1930; goto _test_eof; 
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
	_test_eof1931: ( cs) = 1931; goto _test_eof; 
	_test_eof1932: ( cs) = 1932; goto _test_eof; 
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
	_test_eof1340: ( cs) = 1340; goto _test_eof; 
	_test_eof1341: ( cs) = 1341; goto _test_eof; 
	_test_eof1342: ( cs) = 1342; goto _test_eof; 
	_test_eof1343: ( cs) = 1343; goto _test_eof; 
	_test_eof1344: ( cs) = 1344; goto _test_eof; 
	_test_eof1345: ( cs) = 1345; goto _test_eof; 
	_test_eof1346: ( cs) = 1346; goto _test_eof; 
	_test_eof1347: ( cs) = 1347; goto _test_eof; 
	_test_eof1348: ( cs) = 1348; goto _test_eof; 
	_test_eof1349: ( cs) = 1349; goto _test_eof; 
	_test_eof1350: ( cs) = 1350; goto _test_eof; 
	_test_eof1351: ( cs) = 1351; goto _test_eof; 
	_test_eof1933: ( cs) = 1933; goto _test_eof; 
	_test_eof1352: ( cs) = 1352; goto _test_eof; 
	_test_eof1353: ( cs) = 1353; goto _test_eof; 
	_test_eof1354: ( cs) = 1354; goto _test_eof; 
	_test_eof1355: ( cs) = 1355; goto _test_eof; 
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
	_test_eof1388: ( cs) = 1388; goto _test_eof; 
	_test_eof1934: ( cs) = 1934; goto _test_eof; 
	_test_eof1389: ( cs) = 1389; goto _test_eof; 
	_test_eof1390: ( cs) = 1390; goto _test_eof; 
	_test_eof1935: ( cs) = 1935; goto _test_eof; 
	_test_eof1391: ( cs) = 1391; goto _test_eof; 
	_test_eof1392: ( cs) = 1392; goto _test_eof; 
	_test_eof1393: ( cs) = 1393; goto _test_eof; 
	_test_eof1394: ( cs) = 1394; goto _test_eof; 
	_test_eof1936: ( cs) = 1936; goto _test_eof; 
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
	_test_eof1937: ( cs) = 1937; goto _test_eof; 
	_test_eof1405: ( cs) = 1405; goto _test_eof; 
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
	_test_eof1938: ( cs) = 1938; goto _test_eof; 
	_test_eof1939: ( cs) = 1939; goto _test_eof; 
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
	_test_eof1940: ( cs) = 1940; goto _test_eof; 
	_test_eof1941: ( cs) = 1941; goto _test_eof; 
	_test_eof1942: ( cs) = 1942; goto _test_eof; 
	_test_eof1943: ( cs) = 1943; goto _test_eof; 
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
	_test_eof1944: ( cs) = 1944; goto _test_eof; 
	_test_eof1945: ( cs) = 1945; goto _test_eof; 
	_test_eof1946: ( cs) = 1946; goto _test_eof; 
	_test_eof1947: ( cs) = 1947; goto _test_eof; 
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
	_test_eof1948: ( cs) = 1948; goto _test_eof; 
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
	_test_eof1630: ( cs) = 1630; goto _test_eof; 
	_test_eof1631: ( cs) = 1631; goto _test_eof; 
	_test_eof1632: ( cs) = 1632; goto _test_eof; 
	_test_eof1633: ( cs) = 1633; goto _test_eof; 
	_test_eof1634: ( cs) = 1634; goto _test_eof; 
	_test_eof1635: ( cs) = 1635; goto _test_eof; 
	_test_eof1636: ( cs) = 1636; goto _test_eof; 
	_test_eof1637: ( cs) = 1637; goto _test_eof; 
	_test_eof1638: ( cs) = 1638; goto _test_eof; 
	_test_eof1639: ( cs) = 1639; goto _test_eof; 
	_test_eof1640: ( cs) = 1640; goto _test_eof; 
	_test_eof1641: ( cs) = 1641; goto _test_eof; 
	_test_eof1642: ( cs) = 1642; goto _test_eof; 
	_test_eof1643: ( cs) = 1643; goto _test_eof; 
	_test_eof1644: ( cs) = 1644; goto _test_eof; 
	_test_eof1645: ( cs) = 1645; goto _test_eof; 
	_test_eof1646: ( cs) = 1646; goto _test_eof; 
	_test_eof1647: ( cs) = 1647; goto _test_eof; 
	_test_eof1648: ( cs) = 1648; goto _test_eof; 
	_test_eof1649: ( cs) = 1649; goto _test_eof; 
	_test_eof1650: ( cs) = 1650; goto _test_eof; 
	_test_eof1651: ( cs) = 1651; goto _test_eof; 
	_test_eof1652: ( cs) = 1652; goto _test_eof; 
	_test_eof1653: ( cs) = 1653; goto _test_eof; 
	_test_eof1654: ( cs) = 1654; goto _test_eof; 
	_test_eof1655: ( cs) = 1655; goto _test_eof; 
	_test_eof1656: ( cs) = 1656; goto _test_eof; 
	_test_eof1657: ( cs) = 1657; goto _test_eof; 
	_test_eof1658: ( cs) = 1658; goto _test_eof; 
	_test_eof1659: ( cs) = 1659; goto _test_eof; 
	_test_eof1660: ( cs) = 1660; goto _test_eof; 
	_test_eof1661: ( cs) = 1661; goto _test_eof; 
	_test_eof1662: ( cs) = 1662; goto _test_eof; 
	_test_eof1663: ( cs) = 1663; goto _test_eof; 
	_test_eof1664: ( cs) = 1664; goto _test_eof; 
	_test_eof1665: ( cs) = 1665; goto _test_eof; 
	_test_eof1666: ( cs) = 1666; goto _test_eof; 
	_test_eof1667: ( cs) = 1667; goto _test_eof; 
	_test_eof1668: ( cs) = 1668; goto _test_eof; 
	_test_eof1669: ( cs) = 1669; goto _test_eof; 
	_test_eof1670: ( cs) = 1670; goto _test_eof; 
	_test_eof1671: ( cs) = 1671; goto _test_eof; 
	_test_eof1672: ( cs) = 1672; goto _test_eof; 
	_test_eof1673: ( cs) = 1673; goto _test_eof; 
	_test_eof1674: ( cs) = 1674; goto _test_eof; 
	_test_eof1675: ( cs) = 1675; goto _test_eof; 
	_test_eof1676: ( cs) = 1676; goto _test_eof; 
	_test_eof1677: ( cs) = 1677; goto _test_eof; 
	_test_eof1678: ( cs) = 1678; goto _test_eof; 

	_test_eof: {}
	if ( ( p) == ( eof) )
	{
	switch ( ( cs) ) {
	case 1680: goto tr0;
	case 1: goto tr0;
	case 1681: goto tr2103;
	case 2: goto tr3;
	case 1682: goto tr0;
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
	case 1683: goto tr2104;
	case 17: goto tr0;
	case 18: goto tr0;
	case 19: goto tr0;
	case 20: goto tr0;
	case 21: goto tr0;
	case 22: goto tr0;
	case 23: goto tr0;
	case 24: goto tr0;
	case 25: goto tr0;
	case 26: goto tr0;
	case 27: goto tr0;
	case 28: goto tr0;
	case 1684: goto tr2105;
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
	case 45: goto tr0;
	case 46: goto tr0;
	case 1685: goto tr2106;
	case 47: goto tr0;
	case 48: goto tr0;
	case 49: goto tr0;
	case 50: goto tr0;
	case 51: goto tr0;
	case 52: goto tr0;
	case 53: goto tr0;
	case 54: goto tr0;
	case 55: goto tr0;
	case 56: goto tr0;
	case 57: goto tr0;
	case 58: goto tr3;
	case 59: goto tr3;
	case 60: goto tr3;
	case 61: goto tr3;
	case 1686: goto tr2107;
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
	case 75: goto tr3;
	case 76: goto tr3;
	case 1687: goto tr2103;
	case 1688: goto tr2103;
	case 77: goto tr3;
	case 1689: goto tr2108;
	case 1690: goto tr2108;
	case 78: goto tr3;
	case 1691: goto tr2103;
	case 79: goto tr3;
	case 80: goto tr3;
	case 81: goto tr3;
	case 1692: goto tr2111;
	case 1693: goto tr2103;
	case 82: goto tr3;
	case 83: goto tr3;
	case 84: goto tr3;
	case 85: goto tr3;
	case 86: goto tr3;
	case 87: goto tr3;
	case 88: goto tr3;
	case 89: goto tr3;
	case 90: goto tr3;
	case 91: goto tr3;
	case 1694: goto tr2103;
	case 92: goto tr3;
	case 93: goto tr3;
	case 94: goto tr3;
	case 95: goto tr3;
	case 96: goto tr3;
	case 97: goto tr3;
	case 98: goto tr3;
	case 99: goto tr3;
	case 100: goto tr3;
	case 101: goto tr3;
	case 1695: goto tr2119;
	case 102: goto tr3;
	case 103: goto tr3;
	case 104: goto tr3;
	case 105: goto tr3;
	case 106: goto tr3;
	case 107: goto tr3;
	case 108: goto tr3;
	case 1696: goto tr2120;
	case 109: goto tr132;
	case 1697: goto tr2121;
	case 110: goto tr135;
	case 111: goto tr3;
	case 112: goto tr3;
	case 113: goto tr3;
	case 114: goto tr3;
	case 115: goto tr3;
	case 116: goto tr3;
	case 117: goto tr3;
	case 1698: goto tr2122;
	case 118: goto tr145;
	case 119: goto tr3;
	case 120: goto tr3;
	case 121: goto tr3;
	case 122: goto tr3;
	case 123: goto tr3;
	case 124: goto tr3;
	case 125: goto tr3;
	case 126: goto tr3;
	case 127: goto tr3;
	case 1699: goto tr2123;
	case 128: goto tr3;
	case 1700: goto tr2125;
	case 129: goto tr3;
	case 130: goto tr3;
	case 131: goto tr3;
	case 132: goto tr3;
	case 133: goto tr3;
	case 134: goto tr3;
	case 1701: goto tr2126;
	case 135: goto tr3;
	case 136: goto tr3;
	case 137: goto tr3;
	case 1702: goto tr2103;
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
	case 174: goto tr3;
	case 175: goto tr3;
	case 1703: goto tr2103;
	case 1704: goto tr2103;
	case 1706: goto tr2136;
	case 176: goto tr210;
	case 177: goto tr210;
	case 178: goto tr210;
	case 179: goto tr210;
	case 180: goto tr210;
	case 181: goto tr210;
	case 182: goto tr210;
	case 183: goto tr210;
	case 184: goto tr210;
	case 185: goto tr210;
	case 186: goto tr210;
	case 187: goto tr210;
	case 188: goto tr210;
	case 189: goto tr210;
	case 190: goto tr210;
	case 191: goto tr210;
	case 192: goto tr210;
	case 193: goto tr210;
	case 194: goto tr210;
	case 195: goto tr210;
	case 196: goto tr210;
	case 197: goto tr210;
	case 198: goto tr210;
	case 199: goto tr210;
	case 200: goto tr210;
	case 1707: goto tr2136;
	case 201: goto tr210;
	case 202: goto tr210;
	case 203: goto tr210;
	case 204: goto tr210;
	case 205: goto tr210;
	case 206: goto tr210;
	case 207: goto tr210;
	case 208: goto tr210;
	case 209: goto tr210;
	case 210: goto tr210;
	case 211: goto tr210;
	case 212: goto tr210;
	case 213: goto tr210;
	case 214: goto tr210;
	case 215: goto tr210;
	case 1709: goto tr2177;
	case 1710: goto tr2178;
	case 216: goto tr254;
	case 217: goto tr254;
	case 218: goto tr257;
	case 1711: goto tr2177;
	case 1712: goto tr2177;
	case 1713: goto tr254;
	case 219: goto tr254;
	case 1714: goto tr2177;
	case 220: goto tr261;
	case 1715: goto tr2180;
	case 221: goto tr263;
	case 222: goto tr263;
	case 223: goto tr263;
	case 224: goto tr263;
	case 225: goto tr254;
	case 226: goto tr254;
	case 227: goto tr254;
	case 228: goto tr254;
	case 229: goto tr254;
	case 230: goto tr254;
	case 231: goto tr254;
	case 232: goto tr254;
	case 233: goto tr254;
	case 1716: goto tr2187;
	case 234: goto tr254;
	case 235: goto tr254;
	case 236: goto tr254;
	case 237: goto tr254;
	case 238: goto tr254;
	case 239: goto tr263;
	case 240: goto tr254;
	case 241: goto tr254;
	case 242: goto tr254;
	case 243: goto tr254;
	case 244: goto tr254;
	case 245: goto tr254;
	case 1717: goto tr2188;
	case 246: goto tr263;
	case 247: goto tr254;
	case 248: goto tr254;
	case 249: goto tr263;
	case 250: goto tr254;
	case 251: goto tr254;
	case 252: goto tr254;
	case 253: goto tr254;
	case 254: goto tr254;
	case 255: goto tr254;
	case 256: goto tr254;
	case 257: goto tr254;
	case 258: goto tr254;
	case 259: goto tr263;
	case 260: goto tr263;
	case 261: goto tr263;
	case 1718: goto tr2189;
	case 262: goto tr263;
	case 263: goto tr263;
	case 264: goto tr263;
	case 265: goto tr263;
	case 266: goto tr263;
	case 267: goto tr263;
	case 268: goto tr263;
	case 269: goto tr263;
	case 270: goto tr263;
	case 271: goto tr263;
	case 272: goto tr263;
	case 273: goto tr263;
	case 274: goto tr263;
	case 275: goto tr263;
	case 276: goto tr263;
	case 277: goto tr263;
	case 278: goto tr263;
	case 279: goto tr263;
	case 280: goto tr263;
	case 281: goto tr263;
	case 282: goto tr263;
	case 283: goto tr263;
	case 284: goto tr263;
	case 285: goto tr263;
	case 286: goto tr263;
	case 287: goto tr263;
	case 288: goto tr263;
	case 289: goto tr263;
	case 290: goto tr263;
	case 291: goto tr263;
	case 292: goto tr263;
	case 293: goto tr263;
	case 294: goto tr263;
	case 295: goto tr263;
	case 296: goto tr263;
	case 297: goto tr263;
	case 1719: goto tr2190;
	case 298: goto tr348;
	case 299: goto tr348;
	case 300: goto tr348;
	case 301: goto tr254;
	case 302: goto tr348;
	case 303: goto tr348;
	case 304: goto tr348;
	case 305: goto tr254;
	case 306: goto tr263;
	case 307: goto tr263;
	case 1720: goto tr2193;
	case 1721: goto tr2193;
	case 308: goto tr263;
	case 309: goto tr263;
	case 310: goto tr263;
	case 311: goto tr254;
	case 312: goto tr254;
	case 313: goto tr254;
	case 314: goto tr254;
	case 315: goto tr254;
	case 316: goto tr254;
	case 317: goto tr254;
	case 318: goto tr254;
	case 319: goto tr263;
	case 320: goto tr263;
	case 321: goto tr263;
	case 322: goto tr263;
	case 323: goto tr263;
	case 324: goto tr263;
	case 325: goto tr263;
	case 326: goto tr263;
	case 327: goto tr263;
	case 328: goto tr263;
	case 329: goto tr263;
	case 330: goto tr263;
	case 331: goto tr263;
	case 332: goto tr263;
	case 333: goto tr263;
	case 334: goto tr263;
	case 335: goto tr263;
	case 336: goto tr263;
	case 337: goto tr263;
	case 338: goto tr263;
	case 339: goto tr263;
	case 340: goto tr263;
	case 341: goto tr263;
	case 342: goto tr263;
	case 343: goto tr263;
	case 344: goto tr263;
	case 345: goto tr263;
	case 346: goto tr263;
	case 347: goto tr263;
	case 348: goto tr263;
	case 349: goto tr263;
	case 350: goto tr263;
	case 1722: goto tr2189;
	case 351: goto tr263;
	case 352: goto tr263;
	case 353: goto tr263;
	case 354: goto tr263;
	case 355: goto tr263;
	case 356: goto tr263;
	case 357: goto tr263;
	case 358: goto tr263;
	case 359: goto tr263;
	case 360: goto tr263;
	case 361: goto tr263;
	case 362: goto tr263;
	case 363: goto tr263;
	case 364: goto tr263;
	case 365: goto tr263;
	case 366: goto tr263;
	case 367: goto tr263;
	case 368: goto tr263;
	case 369: goto tr263;
	case 370: goto tr263;
	case 371: goto tr263;
	case 372: goto tr263;
	case 373: goto tr263;
	case 374: goto tr263;
	case 375: goto tr263;
	case 376: goto tr263;
	case 377: goto tr263;
	case 378: goto tr263;
	case 379: goto tr263;
	case 380: goto tr263;
	case 381: goto tr263;
	case 382: goto tr263;
	case 383: goto tr263;
	case 384: goto tr263;
	case 385: goto tr263;
	case 386: goto tr263;
	case 387: goto tr263;
	case 388: goto tr263;
	case 389: goto tr263;
	case 390: goto tr263;
	case 391: goto tr263;
	case 392: goto tr263;
	case 393: goto tr263;
	case 394: goto tr263;
	case 395: goto tr263;
	case 396: goto tr263;
	case 397: goto tr263;
	case 398: goto tr263;
	case 399: goto tr263;
	case 400: goto tr263;
	case 1723: goto tr2177;
	case 401: goto tr261;
	case 402: goto tr261;
	case 403: goto tr261;
	case 1724: goto tr2197;
	case 404: goto tr478;
	case 405: goto tr478;
	case 406: goto tr478;
	case 407: goto tr478;
	case 408: goto tr478;
	case 409: goto tr478;
	case 410: goto tr478;
	case 411: goto tr478;
	case 412: goto tr478;
	case 413: goto tr478;
	case 414: goto tr478;
	case 1725: goto tr2197;
	case 415: goto tr478;
	case 416: goto tr478;
	case 417: goto tr478;
	case 418: goto tr478;
	case 419: goto tr478;
	case 420: goto tr478;
	case 421: goto tr478;
	case 422: goto tr478;
	case 423: goto tr478;
	case 424: goto tr478;
	case 425: goto tr478;
	case 426: goto tr254;
	case 427: goto tr254;
	case 1726: goto tr2197;
	case 428: goto tr254;
	case 429: goto tr254;
	case 430: goto tr254;
	case 431: goto tr254;
	case 432: goto tr254;
	case 433: goto tr254;
	case 434: goto tr254;
	case 435: goto tr254;
	case 436: goto tr254;
	case 437: goto tr261;
	case 438: goto tr261;
	case 439: goto tr261;
	case 440: goto tr261;
	case 441: goto tr261;
	case 442: goto tr261;
	case 443: goto tr261;
	case 444: goto tr261;
	case 445: goto tr261;
	case 446: goto tr261;
	case 447: goto tr261;
	case 448: goto tr254;
	case 449: goto tr254;
	case 1727: goto tr2197;
	case 450: goto tr254;
	case 451: goto tr254;
	case 452: goto tr254;
	case 453: goto tr254;
	case 454: goto tr254;
	case 455: goto tr254;
	case 456: goto tr254;
	case 457: goto tr254;
	case 458: goto tr254;
	case 459: goto tr254;
	case 460: goto tr254;
	case 1728: goto tr2197;
	case 461: goto tr261;
	case 462: goto tr261;
	case 463: goto tr261;
	case 464: goto tr261;
	case 465: goto tr261;
	case 466: goto tr261;
	case 467: goto tr261;
	case 468: goto tr261;
	case 469: goto tr261;
	case 470: goto tr261;
	case 471: goto tr261;
	case 1729: goto tr2177;
	case 472: goto tr261;
	case 473: goto tr261;
	case 474: goto tr261;
	case 475: goto tr261;
	case 476: goto tr261;
	case 477: goto tr261;
	case 478: goto tr261;
	case 479: goto tr261;
	case 480: goto tr261;
	case 481: goto tr261;
	case 482: goto tr261;
	case 483: goto tr261;
	case 484: goto tr261;
	case 485: goto tr261;
	case 486: goto tr261;
	case 487: goto tr261;
	case 488: goto tr261;
	case 489: goto tr261;
	case 490: goto tr261;
	case 491: goto tr261;
	case 492: goto tr261;
	case 493: goto tr261;
	case 494: goto tr261;
	case 495: goto tr261;
	case 496: goto tr261;
	case 497: goto tr261;
	case 498: goto tr261;
	case 499: goto tr261;
	case 500: goto tr261;
	case 501: goto tr261;
	case 502: goto tr261;
	case 503: goto tr261;
	case 504: goto tr261;
	case 505: goto tr261;
	case 506: goto tr261;
	case 507: goto tr261;
	case 508: goto tr261;
	case 509: goto tr261;
	case 510: goto tr261;
	case 511: goto tr261;
	case 512: goto tr261;
	case 513: goto tr261;
	case 514: goto tr261;
	case 515: goto tr261;
	case 516: goto tr261;
	case 517: goto tr261;
	case 518: goto tr261;
	case 1730: goto tr2178;
	case 519: goto tr257;
	case 520: goto tr257;
	case 521: goto tr254;
	case 522: goto tr254;
	case 523: goto tr254;
	case 524: goto tr254;
	case 525: goto tr254;
	case 526: goto tr254;
	case 527: goto tr254;
	case 1731: goto tr2209;
	case 1732: goto tr2211;
	case 528: goto tr254;
	case 529: goto tr254;
	case 530: goto tr254;
	case 531: goto tr254;
	case 1733: goto tr2213;
	case 1734: goto tr2215;
	case 532: goto tr254;
	case 533: goto tr254;
	case 534: goto tr257;
	case 535: goto tr257;
	case 536: goto tr257;
	case 537: goto tr257;
	case 538: goto tr257;
	case 539: goto tr257;
	case 540: goto tr257;
	case 541: goto tr257;
	case 1735: goto tr2209;
	case 1736: goto tr2211;
	case 542: goto tr257;
	case 543: goto tr257;
	case 544: goto tr257;
	case 545: goto tr257;
	case 546: goto tr257;
	case 547: goto tr257;
	case 548: goto tr257;
	case 549: goto tr257;
	case 550: goto tr257;
	case 551: goto tr257;
	case 552: goto tr257;
	case 553: goto tr257;
	case 554: goto tr257;
	case 555: goto tr257;
	case 556: goto tr257;
	case 557: goto tr257;
	case 558: goto tr257;
	case 559: goto tr257;
	case 560: goto tr257;
	case 561: goto tr257;
	case 562: goto tr257;
	case 563: goto tr257;
	case 564: goto tr257;
	case 565: goto tr257;
	case 566: goto tr257;
	case 567: goto tr257;
	case 568: goto tr257;
	case 569: goto tr257;
	case 570: goto tr257;
	case 571: goto tr257;
	case 572: goto tr257;
	case 573: goto tr257;
	case 574: goto tr257;
	case 575: goto tr257;
	case 1737: goto tr2219;
	case 1738: goto tr2221;
	case 576: goto tr257;
	case 577: goto tr254;
	case 578: goto tr254;
	case 579: goto tr254;
	case 1739: goto tr2223;
	case 1740: goto tr2225;
	case 580: goto tr257;
	case 581: goto tr254;
	case 582: goto tr254;
	case 583: goto tr254;
	case 584: goto tr254;
	case 585: goto tr254;
	case 586: goto tr254;
	case 587: goto tr254;
	case 588: goto tr254;
	case 589: goto tr254;
	case 590: goto tr254;
	case 591: goto tr254;
	case 1741: goto tr2227;
	case 1742: goto tr2229;
	case 592: goto tr254;
	case 1743: goto tr2231;
	case 1744: goto tr2233;
	case 593: goto tr254;
	case 594: goto tr254;
	case 595: goto tr254;
	case 596: goto tr254;
	case 597: goto tr254;
	case 598: goto tr254;
	case 599: goto tr254;
	case 600: goto tr254;
	case 1745: goto tr2231;
	case 1746: goto tr2233;
	case 601: goto tr254;
	case 1747: goto tr2231;
	case 602: goto tr254;
	case 603: goto tr254;
	case 604: goto tr254;
	case 605: goto tr254;
	case 606: goto tr254;
	case 607: goto tr254;
	case 608: goto tr254;
	case 609: goto tr254;
	case 610: goto tr254;
	case 611: goto tr254;
	case 612: goto tr254;
	case 613: goto tr254;
	case 614: goto tr254;
	case 615: goto tr254;
	case 616: goto tr254;
	case 617: goto tr254;
	case 618: goto tr254;
	case 619: goto tr254;
	case 1748: goto tr2231;
	case 620: goto tr254;
	case 621: goto tr254;
	case 622: goto tr254;
	case 623: goto tr254;
	case 624: goto tr254;
	case 625: goto tr254;
	case 626: goto tr254;
	case 627: goto tr254;
	case 628: goto tr254;
	case 1749: goto tr2178;
	case 1750: goto tr2178;
	case 1751: goto tr2178;
	case 1752: goto tr2178;
	case 1753: goto tr2178;
	case 629: goto tr257;
	case 630: goto tr257;
	case 1754: goto tr2245;
	case 1755: goto tr2178;
	case 1756: goto tr2178;
	case 1757: goto tr2178;
	case 1758: goto tr2178;
	case 1759: goto tr2178;
	case 631: goto tr257;
	case 632: goto tr257;
	case 1760: goto tr2252;
	case 633: goto tr257;
	case 634: goto tr257;
	case 635: goto tr257;
	case 636: goto tr257;
	case 637: goto tr257;
	case 638: goto tr257;
	case 639: goto tr257;
	case 640: goto tr257;
	case 641: goto tr257;
	case 1761: goto tr2254;
	case 1762: goto tr2178;
	case 1763: goto tr2178;
	case 1764: goto tr2178;
	case 1765: goto tr2178;
	case 642: goto tr257;
	case 643: goto tr257;
	case 644: goto tr257;
	case 645: goto tr257;
	case 646: goto tr257;
	case 647: goto tr257;
	case 648: goto tr257;
	case 649: goto tr257;
	case 650: goto tr257;
	case 651: goto tr257;
	case 1766: goto tr2260;
	case 1767: goto tr2178;
	case 1768: goto tr2178;
	case 1769: goto tr2178;
	case 652: goto tr257;
	case 653: goto tr257;
	case 1770: goto tr2266;
	case 1771: goto tr2178;
	case 1772: goto tr2178;
	case 654: goto tr257;
	case 655: goto tr257;
	case 1773: goto tr2270;
	case 1774: goto tr2178;
	case 1775: goto tr2178;
	case 1776: goto tr2178;
	case 1777: goto tr2178;
	case 1778: goto tr2178;
	case 1779: goto tr2178;
	case 1780: goto tr2178;
	case 656: goto tr257;
	case 657: goto tr257;
	case 1781: goto tr2280;
	case 1782: goto tr2178;
	case 1783: goto tr2178;
	case 658: goto tr257;
	case 659: goto tr257;
	case 1784: goto tr2284;
	case 1785: goto tr2178;
	case 1786: goto tr2178;
	case 1787: goto tr2178;
	case 1788: goto tr2178;
	case 1789: goto tr2178;
	case 660: goto tr257;
	case 661: goto tr257;
	case 1790: goto tr2292;
	case 662: goto tr830;
	case 1791: goto tr2295;
	case 1792: goto tr2178;
	case 1793: goto tr2178;
	case 663: goto tr257;
	case 664: goto tr257;
	case 1794: goto tr2299;
	case 1795: goto tr2178;
	case 1796: goto tr2178;
	case 1797: goto tr2178;
	case 1798: goto tr2178;
	case 665: goto tr257;
	case 666: goto tr257;
	case 1799: goto tr2306;
	case 1800: goto tr2178;
	case 1801: goto tr2178;
	case 1802: goto tr2178;
	case 1803: goto tr2178;
	case 667: goto tr257;
	case 668: goto tr257;
	case 669: goto tr257;
	case 1804: goto tr2313;
	case 670: goto tr257;
	case 671: goto tr257;
	case 672: goto tr257;
	case 673: goto tr257;
	case 674: goto tr257;
	case 675: goto tr257;
	case 676: goto tr257;
	case 677: goto tr257;
	case 678: goto tr257;
	case 679: goto tr257;
	case 680: goto tr257;
	case 1805: goto tr2315;
	case 681: goto tr851;
	case 682: goto tr851;
	case 1806: goto tr2318;
	case 1807: goto tr2178;
	case 1808: goto tr2178;
	case 1809: goto tr2178;
	case 1810: goto tr2178;
	case 1811: goto tr2178;
	case 1812: goto tr2178;
	case 1813: goto tr2178;
	case 1814: goto tr2178;
	case 683: goto tr257;
	case 684: goto tr257;
	case 685: goto tr257;
	case 686: goto tr257;
	case 687: goto tr257;
	case 688: goto tr257;
	case 689: goto tr257;
	case 690: goto tr254;
	case 691: goto tr254;
	case 1815: goto tr2328;
	case 692: goto tr254;
	case 693: goto tr254;
	case 694: goto tr254;
	case 695: goto tr254;
	case 696: goto tr254;
	case 697: goto tr254;
	case 698: goto tr254;
	case 699: goto tr254;
	case 700: goto tr254;
	case 701: goto tr254;
	case 1816: goto tr2328;
	case 702: goto tr877;
	case 703: goto tr877;
	case 704: goto tr877;
	case 705: goto tr877;
	case 706: goto tr877;
	case 707: goto tr877;
	case 708: goto tr877;
	case 709: goto tr877;
	case 710: goto tr877;
	case 711: goto tr877;
	case 712: goto tr877;
	case 1817: goto tr2328;
	case 713: goto tr877;
	case 714: goto tr877;
	case 715: goto tr877;
	case 716: goto tr877;
	case 717: goto tr877;
	case 718: goto tr877;
	case 719: goto tr877;
	case 720: goto tr877;
	case 721: goto tr877;
	case 722: goto tr877;
	case 723: goto tr877;
	case 724: goto tr254;
	case 725: goto tr254;
	case 1818: goto tr2328;
	case 726: goto tr254;
	case 727: goto tr254;
	case 728: goto tr254;
	case 729: goto tr254;
	case 730: goto tr254;
	case 731: goto tr254;
	case 732: goto tr254;
	case 733: goto tr254;
	case 734: goto tr254;
	case 735: goto tr254;
	case 1819: goto tr2328;
	case 1820: goto tr2178;
	case 1821: goto tr2178;
	case 1822: goto tr2178;
	case 1823: goto tr2178;
	case 1824: goto tr2178;
	case 1825: goto tr2178;
	case 1826: goto tr2178;
	case 1827: goto tr2178;
	case 1828: goto tr2178;
	case 1829: goto tr2178;
	case 1830: goto tr2178;
	case 1831: goto tr2178;
	case 736: goto tr257;
	case 737: goto tr257;
	case 1832: goto tr2341;
	case 1833: goto tr2178;
	case 1834: goto tr2178;
	case 1835: goto tr2178;
	case 1836: goto tr2178;
	case 738: goto tr257;
	case 739: goto tr257;
	case 1837: goto tr2347;
	case 1838: goto tr2178;
	case 1839: goto tr2178;
	case 1840: goto tr2178;
	case 740: goto tr257;
	case 741: goto tr257;
	case 742: goto tr257;
	case 743: goto tr257;
	case 744: goto tr257;
	case 745: goto tr257;
	case 746: goto tr257;
	case 747: goto tr257;
	case 748: goto tr257;
	case 1841: goto tr2352;
	case 1842: goto tr2178;
	case 1843: goto tr2178;
	case 1844: goto tr2178;
	case 1845: goto tr2178;
	case 749: goto tr257;
	case 750: goto tr257;
	case 1846: goto tr2358;
	case 1847: goto tr2178;
	case 1848: goto tr2178;
	case 1849: goto tr2178;
	case 1850: goto tr2178;
	case 751: goto tr257;
	case 752: goto tr257;
	case 1851: goto tr2366;
	case 1852: goto tr2178;
	case 1853: goto tr2178;
	case 753: goto tr257;
	case 754: goto tr257;
	case 1854: goto tr2370;
	case 755: goto tr257;
	case 756: goto tr257;
	case 757: goto tr257;
	case 758: goto tr257;
	case 759: goto tr257;
	case 760: goto tr257;
	case 761: goto tr257;
	case 762: goto tr257;
	case 763: goto tr257;
	case 1855: goto tr2372;
	case 1856: goto tr2178;
	case 1857: goto tr2178;
	case 1858: goto tr2178;
	case 764: goto tr257;
	case 765: goto tr257;
	case 1859: goto tr2377;
	case 1860: goto tr2178;
	case 1861: goto tr2178;
	case 1862: goto tr2178;
	case 1863: goto tr2178;
	case 1864: goto tr2178;
	case 1865: goto tr2178;
	case 766: goto tr257;
	case 767: goto tr257;
	case 1866: goto tr2385;
	case 1867: goto tr2178;
	case 1868: goto tr2178;
	case 1869: goto tr2178;
	case 768: goto tr257;
	case 769: goto tr257;
	case 1870: goto tr2390;
	case 1871: goto tr2178;
	case 1872: goto tr2178;
	case 1873: goto tr2178;
	case 1874: goto tr2178;
	case 770: goto tr257;
	case 771: goto tr257;
	case 772: goto tr257;
	case 773: goto tr257;
	case 774: goto tr257;
	case 775: goto tr257;
	case 776: goto tr257;
	case 1875: goto tr2400;
	case 777: goto tr257;
	case 778: goto tr257;
	case 779: goto tr257;
	case 780: goto tr257;
	case 781: goto tr257;
	case 782: goto tr257;
	case 783: goto tr257;
	case 784: goto tr257;
	case 1876: goto tr2178;
	case 1877: goto tr2178;
	case 1878: goto tr2178;
	case 1879: goto tr2178;
	case 1880: goto tr2178;
	case 1881: goto tr2178;
	case 1882: goto tr2178;
	case 1883: goto tr2178;
	case 785: goto tr257;
	case 786: goto tr257;
	case 1884: goto tr2409;
	case 1885: goto tr2178;
	case 1886: goto tr2178;
	case 1887: goto tr2178;
	case 1888: goto tr2178;
	case 1889: goto tr2178;
	case 787: goto tr257;
	case 788: goto tr257;
	case 1890: goto tr2416;
	case 1891: goto tr2178;
	case 1892: goto tr2178;
	case 1893: goto tr2178;
	case 1894: goto tr2178;
	case 1895: goto tr2178;
	case 1896: goto tr2178;
	case 1897: goto tr2178;
	case 1898: goto tr2178;
	case 789: goto tr257;
	case 790: goto tr257;
	case 1899: goto tr2425;
	case 1900: goto tr2178;
	case 1901: goto tr2178;
	case 1902: goto tr2178;
	case 1903: goto tr2178;
	case 791: goto tr257;
	case 792: goto tr257;
	case 793: goto tr257;
	case 1904: goto tr2433;
	case 794: goto tr257;
	case 795: goto tr257;
	case 796: goto tr257;
	case 797: goto tr257;
	case 798: goto tr257;
	case 799: goto tr257;
	case 800: goto tr257;
	case 801: goto tr257;
	case 802: goto tr257;
	case 1905: goto tr2435;
	case 803: goto tr257;
	case 804: goto tr257;
	case 805: goto tr257;
	case 806: goto tr257;
	case 807: goto tr257;
	case 808: goto tr257;
	case 1906: goto tr2433;
	case 1907: goto tr2178;
	case 1908: goto tr2178;
	case 1909: goto tr2178;
	case 1910: goto tr2178;
	case 1911: goto tr2178;
	case 1912: goto tr2178;
	case 1913: goto tr2178;
	case 1914: goto tr2178;
	case 1915: goto tr2178;
	case 1916: goto tr2178;
	case 1917: goto tr2178;
	case 1918: goto tr2177;
	case 809: goto tr261;
	case 810: goto tr261;
	case 811: goto tr254;
	case 812: goto tr254;
	case 813: goto tr254;
	case 814: goto tr254;
	case 815: goto tr254;
	case 816: goto tr254;
	case 817: goto tr254;
	case 818: goto tr254;
	case 819: goto tr254;
	case 820: goto tr254;
	case 821: goto tr261;
	case 822: goto tr261;
	case 823: goto tr261;
	case 824: goto tr254;
	case 825: goto tr254;
	case 826: goto tr254;
	case 827: goto tr254;
	case 828: goto tr254;
	case 829: goto tr254;
	case 830: goto tr254;
	case 831: goto tr254;
	case 832: goto tr254;
	case 833: goto tr254;
	case 1919: goto tr254;
	case 834: goto tr254;
	case 835: goto tr261;
	case 1920: goto tr254;
	case 836: goto tr261;
	case 837: goto tr261;
	case 838: goto tr261;
	case 839: goto tr261;
	case 840: goto tr261;
	case 841: goto tr261;
	case 842: goto tr261;
	case 843: goto tr261;
	case 844: goto tr261;
	case 845: goto tr261;
	case 846: goto tr261;
	case 847: goto tr261;
	case 848: goto tr261;
	case 849: goto tr261;
	case 850: goto tr261;
	case 851: goto tr261;
	case 852: goto tr261;
	case 853: goto tr261;
	case 854: goto tr261;
	case 855: goto tr261;
	case 856: goto tr261;
	case 857: goto tr261;
	case 858: goto tr261;
	case 859: goto tr261;
	case 860: goto tr261;
	case 861: goto tr261;
	case 862: goto tr261;
	case 863: goto tr261;
	case 864: goto tr261;
	case 865: goto tr261;
	case 866: goto tr261;
	case 867: goto tr261;
	case 868: goto tr261;
	case 869: goto tr261;
	case 870: goto tr261;
	case 871: goto tr261;
	case 872: goto tr261;
	case 873: goto tr261;
	case 874: goto tr261;
	case 875: goto tr261;
	case 876: goto tr261;
	case 877: goto tr261;
	case 878: goto tr261;
	case 879: goto tr261;
	case 880: goto tr261;
	case 881: goto tr261;
	case 882: goto tr261;
	case 883: goto tr261;
	case 884: goto tr261;
	case 885: goto tr261;
	case 886: goto tr261;
	case 887: goto tr261;
	case 888: goto tr261;
	case 889: goto tr261;
	case 890: goto tr261;
	case 891: goto tr261;
	case 892: goto tr261;
	case 893: goto tr261;
	case 894: goto tr261;
	case 895: goto tr261;
	case 896: goto tr261;
	case 897: goto tr261;
	case 898: goto tr261;
	case 899: goto tr261;
	case 900: goto tr261;
	case 1921: goto tr2460;
	case 901: goto tr1098;
	case 1922: goto tr2461;
	case 902: goto tr1101;
	case 903: goto tr261;
	case 904: goto tr261;
	case 905: goto tr261;
	case 906: goto tr261;
	case 907: goto tr261;
	case 908: goto tr261;
	case 909: goto tr261;
	case 910: goto tr261;
	case 911: goto tr261;
	case 912: goto tr261;
	case 913: goto tr261;
	case 914: goto tr261;
	case 915: goto tr261;
	case 916: goto tr261;
	case 917: goto tr261;
	case 918: goto tr261;
	case 1923: goto tr254;
	case 919: goto tr261;
	case 920: goto tr261;
	case 921: goto tr261;
	case 922: goto tr261;
	case 923: goto tr261;
	case 924: goto tr261;
	case 925: goto tr261;
	case 926: goto tr261;
	case 927: goto tr261;
	case 928: goto tr261;
	case 929: goto tr261;
	case 930: goto tr261;
	case 931: goto tr261;
	case 932: goto tr261;
	case 933: goto tr261;
	case 934: goto tr261;
	case 935: goto tr261;
	case 936: goto tr261;
	case 937: goto tr261;
	case 938: goto tr261;
	case 939: goto tr261;
	case 940: goto tr261;
	case 941: goto tr261;
	case 942: goto tr261;
	case 943: goto tr261;
	case 944: goto tr261;
	case 945: goto tr261;
	case 946: goto tr261;
	case 947: goto tr261;
	case 948: goto tr261;
	case 949: goto tr261;
	case 950: goto tr261;
	case 951: goto tr261;
	case 952: goto tr261;
	case 953: goto tr261;
	case 954: goto tr261;
	case 955: goto tr261;
	case 956: goto tr261;
	case 957: goto tr261;
	case 958: goto tr261;
	case 959: goto tr261;
	case 960: goto tr261;
	case 961: goto tr261;
	case 962: goto tr261;
	case 963: goto tr261;
	case 964: goto tr261;
	case 965: goto tr261;
	case 966: goto tr261;
	case 967: goto tr261;
	case 968: goto tr261;
	case 969: goto tr261;
	case 970: goto tr261;
	case 971: goto tr261;
	case 972: goto tr261;
	case 973: goto tr261;
	case 974: goto tr261;
	case 975: goto tr261;
	case 976: goto tr261;
	case 977: goto tr261;
	case 978: goto tr261;
	case 979: goto tr261;
	case 980: goto tr261;
	case 981: goto tr261;
	case 982: goto tr261;
	case 983: goto tr261;
	case 984: goto tr261;
	case 985: goto tr261;
	case 986: goto tr261;
	case 987: goto tr261;
	case 988: goto tr261;
	case 989: goto tr261;
	case 990: goto tr261;
	case 991: goto tr261;
	case 992: goto tr261;
	case 993: goto tr261;
	case 994: goto tr261;
	case 995: goto tr261;
	case 996: goto tr261;
	case 997: goto tr261;
	case 998: goto tr261;
	case 999: goto tr261;
	case 1000: goto tr261;
	case 1001: goto tr261;
	case 1002: goto tr261;
	case 1003: goto tr261;
	case 1004: goto tr261;
	case 1005: goto tr261;
	case 1006: goto tr261;
	case 1007: goto tr261;
	case 1008: goto tr261;
	case 1009: goto tr261;
	case 1010: goto tr261;
	case 1011: goto tr261;
	case 1012: goto tr261;
	case 1013: goto tr261;
	case 1014: goto tr261;
	case 1015: goto tr261;
	case 1016: goto tr261;
	case 1017: goto tr261;
	case 1018: goto tr261;
	case 1019: goto tr261;
	case 1020: goto tr261;
	case 1021: goto tr261;
	case 1022: goto tr261;
	case 1023: goto tr261;
	case 1024: goto tr261;
	case 1025: goto tr261;
	case 1026: goto tr261;
	case 1027: goto tr261;
	case 1028: goto tr261;
	case 1029: goto tr261;
	case 1030: goto tr261;
	case 1031: goto tr261;
	case 1032: goto tr261;
	case 1033: goto tr261;
	case 1034: goto tr261;
	case 1035: goto tr261;
	case 1036: goto tr261;
	case 1037: goto tr261;
	case 1038: goto tr261;
	case 1039: goto tr261;
	case 1040: goto tr261;
	case 1041: goto tr261;
	case 1042: goto tr261;
	case 1043: goto tr261;
	case 1044: goto tr261;
	case 1045: goto tr261;
	case 1046: goto tr261;
	case 1047: goto tr261;
	case 1048: goto tr261;
	case 1049: goto tr261;
	case 1050: goto tr261;
	case 1051: goto tr261;
	case 1052: goto tr261;
	case 1053: goto tr261;
	case 1054: goto tr261;
	case 1055: goto tr261;
	case 1056: goto tr261;
	case 1057: goto tr261;
	case 1058: goto tr261;
	case 1059: goto tr261;
	case 1060: goto tr261;
	case 1061: goto tr261;
	case 1062: goto tr261;
	case 1063: goto tr261;
	case 1064: goto tr261;
	case 1065: goto tr261;
	case 1066: goto tr261;
	case 1067: goto tr261;
	case 1068: goto tr261;
	case 1069: goto tr254;
	case 1070: goto tr254;
	case 1071: goto tr254;
	case 1072: goto tr261;
	case 1073: goto tr261;
	case 1074: goto tr261;
	case 1924: goto tr2462;
	case 1075: goto tr261;
	case 1076: goto tr261;
	case 1925: goto tr2462;
	case 1077: goto tr261;
	case 1078: goto tr261;
	case 1079: goto tr261;
	case 1080: goto tr261;
	case 1081: goto tr261;
	case 1082: goto tr261;
	case 1083: goto tr261;
	case 1084: goto tr261;
	case 1085: goto tr261;
	case 1086: goto tr261;
	case 1087: goto tr261;
	case 1088: goto tr261;
	case 1089: goto tr261;
	case 1090: goto tr261;
	case 1091: goto tr261;
	case 1092: goto tr261;
	case 1093: goto tr261;
	case 1926: goto tr2463;
	case 1094: goto tr1300;
	case 1095: goto tr261;
	case 1096: goto tr261;
	case 1097: goto tr261;
	case 1098: goto tr261;
	case 1099: goto tr261;
	case 1100: goto tr261;
	case 1101: goto tr261;
	case 1102: goto tr261;
	case 1103: goto tr261;
	case 1104: goto tr261;
	case 1105: goto tr261;
	case 1106: goto tr261;
	case 1107: goto tr261;
	case 1108: goto tr261;
	case 1109: goto tr261;
	case 1110: goto tr261;
	case 1111: goto tr261;
	case 1112: goto tr261;
	case 1113: goto tr261;
	case 1114: goto tr261;
	case 1115: goto tr261;
	case 1116: goto tr261;
	case 1117: goto tr261;
	case 1118: goto tr261;
	case 1119: goto tr261;
	case 1120: goto tr261;
	case 1121: goto tr261;
	case 1122: goto tr261;
	case 1123: goto tr261;
	case 1124: goto tr261;
	case 1125: goto tr261;
	case 1126: goto tr261;
	case 1127: goto tr261;
	case 1128: goto tr261;
	case 1129: goto tr261;
	case 1130: goto tr261;
	case 1131: goto tr261;
	case 1132: goto tr261;
	case 1133: goto tr261;
	case 1134: goto tr261;
	case 1135: goto tr261;
	case 1136: goto tr254;
	case 1137: goto tr254;
	case 1138: goto tr254;
	case 1139: goto tr254;
	case 1140: goto tr254;
	case 1141: goto tr254;
	case 1142: goto tr254;
	case 1143: goto tr254;
	case 1144: goto tr261;
	case 1145: goto tr261;
	case 1927: goto tr2462;
	case 1146: goto tr261;
	case 1147: goto tr261;
	case 1148: goto tr261;
	case 1149: goto tr261;
	case 1150: goto tr261;
	case 1151: goto tr261;
	case 1152: goto tr261;
	case 1153: goto tr261;
	case 1154: goto tr261;
	case 1155: goto tr261;
	case 1156: goto tr261;
	case 1157: goto tr261;
	case 1158: goto tr261;
	case 1159: goto tr261;
	case 1160: goto tr254;
	case 1161: goto tr254;
	case 1162: goto tr261;
	case 1163: goto tr261;
	case 1164: goto tr261;
	case 1165: goto tr261;
	case 1928: goto tr2462;
	case 1166: goto tr261;
	case 1167: goto tr261;
	case 1168: goto tr261;
	case 1169: goto tr261;
	case 1170: goto tr261;
	case 1171: goto tr261;
	case 1172: goto tr261;
	case 1173: goto tr261;
	case 1174: goto tr261;
	case 1175: goto tr261;
	case 1176: goto tr261;
	case 1177: goto tr261;
	case 1178: goto tr261;
	case 1179: goto tr261;
	case 1180: goto tr261;
	case 1181: goto tr261;
	case 1182: goto tr261;
	case 1183: goto tr261;
	case 1184: goto tr261;
	case 1185: goto tr261;
	case 1186: goto tr254;
	case 1187: goto tr261;
	case 1188: goto tr261;
	case 1929: goto tr2462;
	case 1189: goto tr261;
	case 1190: goto tr261;
	case 1191: goto tr261;
	case 1192: goto tr261;
	case 1193: goto tr261;
	case 1194: goto tr261;
	case 1195: goto tr261;
	case 1196: goto tr261;
	case 1197: goto tr261;
	case 1198: goto tr261;
	case 1199: goto tr261;
	case 1200: goto tr261;
	case 1201: goto tr261;
	case 1202: goto tr261;
	case 1203: goto tr261;
	case 1204: goto tr261;
	case 1205: goto tr261;
	case 1206: goto tr261;
	case 1207: goto tr261;
	case 1208: goto tr261;
	case 1209: goto tr261;
	case 1210: goto tr261;
	case 1211: goto tr261;
	case 1212: goto tr261;
	case 1213: goto tr254;
	case 1214: goto tr261;
	case 1215: goto tr261;
	case 1930: goto tr2462;
	case 1216: goto tr261;
	case 1217: goto tr261;
	case 1218: goto tr261;
	case 1219: goto tr261;
	case 1220: goto tr261;
	case 1221: goto tr261;
	case 1222: goto tr261;
	case 1223: goto tr261;
	case 1224: goto tr261;
	case 1225: goto tr261;
	case 1226: goto tr261;
	case 1227: goto tr261;
	case 1228: goto tr261;
	case 1229: goto tr261;
	case 1230: goto tr261;
	case 1231: goto tr261;
	case 1232: goto tr261;
	case 1233: goto tr261;
	case 1234: goto tr261;
	case 1235: goto tr261;
	case 1236: goto tr261;
	case 1237: goto tr261;
	case 1238: goto tr261;
	case 1239: goto tr261;
	case 1240: goto tr261;
	case 1241: goto tr261;
	case 1242: goto tr261;
	case 1243: goto tr261;
	case 1244: goto tr261;
	case 1245: goto tr261;
	case 1246: goto tr261;
	case 1247: goto tr261;
	case 1248: goto tr261;
	case 1249: goto tr261;
	case 1250: goto tr261;
	case 1251: goto tr261;
	case 1252: goto tr261;
	case 1253: goto tr261;
	case 1254: goto tr261;
	case 1255: goto tr261;
	case 1256: goto tr261;
	case 1257: goto tr261;
	case 1258: goto tr261;
	case 1259: goto tr261;
	case 1260: goto tr261;
	case 1261: goto tr261;
	case 1262: goto tr261;
	case 1263: goto tr261;
	case 1264: goto tr261;
	case 1265: goto tr261;
	case 1266: goto tr261;
	case 1267: goto tr261;
	case 1268: goto tr261;
	case 1269: goto tr261;
	case 1270: goto tr261;
	case 1271: goto tr261;
	case 1272: goto tr261;
	case 1273: goto tr261;
	case 1274: goto tr261;
	case 1275: goto tr261;
	case 1276: goto tr261;
	case 1277: goto tr261;
	case 1278: goto tr261;
	case 1279: goto tr261;
	case 1280: goto tr261;
	case 1281: goto tr261;
	case 1282: goto tr261;
	case 1283: goto tr261;
	case 1284: goto tr261;
	case 1285: goto tr261;
	case 1286: goto tr261;
	case 1287: goto tr261;
	case 1288: goto tr261;
	case 1289: goto tr261;
	case 1290: goto tr261;
	case 1291: goto tr261;
	case 1292: goto tr261;
	case 1293: goto tr261;
	case 1294: goto tr261;
	case 1295: goto tr261;
	case 1296: goto tr261;
	case 1297: goto tr261;
	case 1298: goto tr261;
	case 1299: goto tr261;
	case 1300: goto tr261;
	case 1301: goto tr261;
	case 1302: goto tr261;
	case 1303: goto tr261;
	case 1304: goto tr261;
	case 1305: goto tr261;
	case 1931: goto tr2177;
	case 1932: goto tr2177;
	case 1306: goto tr261;
	case 1307: goto tr261;
	case 1308: goto tr261;
	case 1309: goto tr261;
	case 1310: goto tr261;
	case 1311: goto tr261;
	case 1312: goto tr261;
	case 1313: goto tr261;
	case 1314: goto tr261;
	case 1315: goto tr261;
	case 1316: goto tr261;
	case 1317: goto tr261;
	case 1318: goto tr261;
	case 1319: goto tr261;
	case 1320: goto tr261;
	case 1321: goto tr261;
	case 1322: goto tr261;
	case 1323: goto tr261;
	case 1324: goto tr261;
	case 1325: goto tr261;
	case 1326: goto tr261;
	case 1327: goto tr261;
	case 1328: goto tr261;
	case 1329: goto tr261;
	case 1330: goto tr261;
	case 1331: goto tr261;
	case 1332: goto tr261;
	case 1333: goto tr261;
	case 1334: goto tr261;
	case 1335: goto tr261;
	case 1336: goto tr261;
	case 1337: goto tr261;
	case 1338: goto tr261;
	case 1339: goto tr261;
	case 1340: goto tr261;
	case 1341: goto tr261;
	case 1342: goto tr261;
	case 1343: goto tr261;
	case 1344: goto tr261;
	case 1345: goto tr261;
	case 1346: goto tr261;
	case 1347: goto tr261;
	case 1348: goto tr261;
	case 1349: goto tr261;
	case 1350: goto tr261;
	case 1351: goto tr261;
	case 1933: goto tr2460;
	case 1352: goto tr261;
	case 1353: goto tr261;
	case 1354: goto tr261;
	case 1355: goto tr261;
	case 1356: goto tr261;
	case 1357: goto tr261;
	case 1358: goto tr261;
	case 1359: goto tr261;
	case 1360: goto tr261;
	case 1361: goto tr261;
	case 1362: goto tr261;
	case 1363: goto tr261;
	case 1364: goto tr261;
	case 1365: goto tr261;
	case 1366: goto tr261;
	case 1367: goto tr261;
	case 1368: goto tr261;
	case 1369: goto tr261;
	case 1370: goto tr261;
	case 1371: goto tr261;
	case 1372: goto tr261;
	case 1373: goto tr261;
	case 1374: goto tr261;
	case 1375: goto tr261;
	case 1376: goto tr261;
	case 1377: goto tr261;
	case 1378: goto tr261;
	case 1379: goto tr261;
	case 1380: goto tr261;
	case 1381: goto tr261;
	case 1382: goto tr261;
	case 1383: goto tr261;
	case 1384: goto tr261;
	case 1385: goto tr261;
	case 1386: goto tr261;
	case 1387: goto tr261;
	case 1388: goto tr261;
	case 1934: goto tr2177;
	case 1389: goto tr261;
	case 1390: goto tr261;
	case 1935: goto tr2177;
	case 1391: goto tr261;
	case 1392: goto tr254;
	case 1393: goto tr254;
	case 1394: goto tr254;
	case 1936: goto tr2483;
	case 1395: goto tr254;
	case 1396: goto tr254;
	case 1397: goto tr254;
	case 1398: goto tr254;
	case 1399: goto tr254;
	case 1400: goto tr254;
	case 1401: goto tr254;
	case 1402: goto tr254;
	case 1403: goto tr254;
	case 1404: goto tr254;
	case 1937: goto tr2483;
	case 1405: goto tr254;
	case 1406: goto tr254;
	case 1407: goto tr254;
	case 1408: goto tr254;
	case 1409: goto tr254;
	case 1410: goto tr254;
	case 1411: goto tr254;
	case 1412: goto tr254;
	case 1413: goto tr254;
	case 1414: goto tr254;
	case 1415: goto tr261;
	case 1416: goto tr261;
	case 1417: goto tr261;
	case 1418: goto tr261;
	case 1419: goto tr261;
	case 1420: goto tr261;
	case 1421: goto tr261;
	case 1422: goto tr261;
	case 1423: goto tr261;
	case 1424: goto tr261;
	case 1939: goto tr2489;
	case 1425: goto tr1705;
	case 1426: goto tr1705;
	case 1427: goto tr1705;
	case 1428: goto tr1705;
	case 1429: goto tr1705;
	case 1430: goto tr1705;
	case 1431: goto tr1705;
	case 1432: goto tr1705;
	case 1433: goto tr1705;
	case 1434: goto tr1705;
	case 1435: goto tr1705;
	case 1436: goto tr1705;
	case 1940: goto tr2489;
	case 1941: goto tr2489;
	case 1943: goto tr2497;
	case 1437: goto tr1717;
	case 1438: goto tr1717;
	case 1439: goto tr1717;
	case 1440: goto tr1717;
	case 1441: goto tr1717;
	case 1442: goto tr1717;
	case 1443: goto tr1717;
	case 1444: goto tr1717;
	case 1445: goto tr1717;
	case 1446: goto tr1717;
	case 1447: goto tr1717;
	case 1448: goto tr1717;
	case 1449: goto tr1717;
	case 1450: goto tr1717;
	case 1451: goto tr1717;
	case 1452: goto tr1717;
	case 1453: goto tr1717;
	case 1454: goto tr1717;
	case 1944: goto tr2497;
	case 1945: goto tr2497;
	case 1947: goto tr2503;
	case 1455: goto tr1735;
	case 1456: goto tr1735;
	case 1457: goto tr1735;
	case 1458: goto tr1735;
	case 1459: goto tr1735;
	case 1460: goto tr1735;
	case 1461: goto tr1735;
	case 1462: goto tr1735;
	case 1463: goto tr1735;
	case 1464: goto tr1735;
	case 1465: goto tr1735;
	case 1466: goto tr1735;
	case 1467: goto tr1735;
	case 1468: goto tr1735;
	case 1469: goto tr1735;
	case 1470: goto tr1735;
	case 1471: goto tr1735;
	case 1472: goto tr1735;
	case 1473: goto tr1735;
	case 1474: goto tr1735;
	case 1475: goto tr1735;
	case 1476: goto tr1735;
	case 1477: goto tr1735;
	case 1478: goto tr1735;
	case 1479: goto tr1735;
	case 1480: goto tr1735;
	case 1481: goto tr1735;
	case 1482: goto tr1735;
	case 1483: goto tr1735;
	case 1484: goto tr1735;
	case 1485: goto tr1735;
	case 1486: goto tr1735;
	case 1487: goto tr1735;
	case 1488: goto tr1735;
	case 1489: goto tr1735;
	case 1490: goto tr1735;
	case 1491: goto tr1735;
	case 1492: goto tr1735;
	case 1493: goto tr1735;
	case 1494: goto tr1735;
	case 1495: goto tr1735;
	case 1496: goto tr1735;
	case 1497: goto tr1735;
	case 1498: goto tr1735;
	case 1499: goto tr1735;
	case 1500: goto tr1735;
	case 1501: goto tr1735;
	case 1502: goto tr1735;
	case 1503: goto tr1735;
	case 1504: goto tr1735;
	case 1505: goto tr1735;
	case 1506: goto tr1735;
	case 1507: goto tr1735;
	case 1508: goto tr1735;
	case 1509: goto tr1735;
	case 1510: goto tr1735;
	case 1511: goto tr1735;
	case 1512: goto tr1735;
	case 1513: goto tr1735;
	case 1514: goto tr1735;
	case 1515: goto tr1735;
	case 1516: goto tr1735;
	case 1517: goto tr1735;
	case 1518: goto tr1735;
	case 1519: goto tr1735;
	case 1520: goto tr1735;
	case 1521: goto tr1735;
	case 1522: goto tr1735;
	case 1523: goto tr1735;
	case 1524: goto tr1735;
	case 1525: goto tr1735;
	case 1526: goto tr1735;
	case 1527: goto tr1735;
	case 1528: goto tr1735;
	case 1529: goto tr1735;
	case 1530: goto tr1735;
	case 1531: goto tr1735;
	case 1532: goto tr1735;
	case 1533: goto tr1735;
	case 1534: goto tr1735;
	case 1535: goto tr1735;
	case 1536: goto tr1735;
	case 1537: goto tr1735;
	case 1538: goto tr1735;
	case 1539: goto tr1735;
	case 1540: goto tr1735;
	case 1541: goto tr1735;
	case 1542: goto tr1735;
	case 1543: goto tr1735;
	case 1544: goto tr1735;
	case 1545: goto tr1735;
	case 1546: goto tr1735;
	case 1547: goto tr1735;
	case 1548: goto tr1735;
	case 1549: goto tr1735;
	case 1550: goto tr1735;
	case 1551: goto tr1735;
	case 1552: goto tr1735;
	case 1553: goto tr1735;
	case 1554: goto tr1735;
	case 1555: goto tr1735;
	case 1556: goto tr1735;
	case 1557: goto tr1735;
	case 1558: goto tr1735;
	case 1559: goto tr1735;
	case 1560: goto tr1735;
	case 1561: goto tr1735;
	case 1562: goto tr1735;
	case 1563: goto tr1735;
	case 1564: goto tr1735;
	case 1565: goto tr1735;
	case 1566: goto tr1735;
	case 1948: goto tr2503;
	case 1567: goto tr1735;
	case 1568: goto tr1735;
	case 1569: goto tr1735;
	case 1570: goto tr1735;
	case 1571: goto tr1735;
	case 1572: goto tr1735;
	case 1573: goto tr1735;
	case 1574: goto tr1735;
	case 1575: goto tr1735;
	case 1576: goto tr1735;
	case 1577: goto tr1735;
	case 1578: goto tr1735;
	case 1579: goto tr1735;
	case 1580: goto tr1735;
	case 1581: goto tr1735;
	case 1582: goto tr1735;
	case 1583: goto tr1735;
	case 1584: goto tr1735;
	case 1585: goto tr1735;
	case 1586: goto tr1735;
	case 1587: goto tr1735;
	case 1588: goto tr1735;
	case 1589: goto tr1735;
	case 1590: goto tr1735;
	case 1591: goto tr1735;
	case 1592: goto tr1735;
	case 1593: goto tr1735;
	case 1594: goto tr1735;
	case 1595: goto tr1735;
	case 1596: goto tr1735;
	case 1597: goto tr1735;
	case 1598: goto tr1735;
	case 1599: goto tr1735;
	case 1600: goto tr1735;
	case 1601: goto tr1735;
	case 1602: goto tr1735;
	case 1603: goto tr1735;
	case 1604: goto tr1735;
	case 1605: goto tr1735;
	case 1606: goto tr1735;
	case 1607: goto tr1735;
	case 1608: goto tr1735;
	case 1609: goto tr1735;
	case 1610: goto tr1735;
	case 1611: goto tr1735;
	case 1612: goto tr1735;
	case 1613: goto tr1735;
	case 1614: goto tr1735;
	case 1615: goto tr1735;
	case 1616: goto tr1735;
	case 1617: goto tr1735;
	case 1618: goto tr1735;
	case 1619: goto tr1735;
	case 1620: goto tr1735;
	case 1621: goto tr1735;
	case 1622: goto tr1735;
	case 1623: goto tr1735;
	case 1624: goto tr1735;
	case 1625: goto tr1735;
	case 1626: goto tr1735;
	case 1627: goto tr1735;
	case 1628: goto tr1735;
	case 1629: goto tr1735;
	case 1630: goto tr1735;
	case 1631: goto tr1735;
	case 1632: goto tr1735;
	case 1633: goto tr1735;
	case 1634: goto tr1735;
	case 1635: goto tr1735;
	case 1636: goto tr1735;
	case 1637: goto tr1735;
	case 1638: goto tr1735;
	case 1639: goto tr1735;
	case 1640: goto tr1735;
	case 1641: goto tr1735;
	case 1642: goto tr1735;
	case 1643: goto tr1735;
	case 1644: goto tr1735;
	case 1645: goto tr1735;
	case 1646: goto tr1735;
	case 1647: goto tr1735;
	case 1648: goto tr1735;
	case 1649: goto tr1735;
	case 1650: goto tr1735;
	case 1651: goto tr1735;
	case 1652: goto tr1735;
	case 1653: goto tr1735;
	case 1654: goto tr1735;
	case 1655: goto tr1735;
	case 1656: goto tr1735;
	case 1657: goto tr1735;
	case 1658: goto tr1735;
	case 1659: goto tr1735;
	case 1660: goto tr1735;
	case 1661: goto tr1735;
	case 1662: goto tr1735;
	case 1663: goto tr1735;
	case 1664: goto tr1735;
	case 1665: goto tr1735;
	case 1666: goto tr1735;
	case 1667: goto tr1735;
	case 1668: goto tr1735;
	case 1669: goto tr1735;
	case 1670: goto tr1735;
	case 1671: goto tr1735;
	case 1672: goto tr1735;
	case 1673: goto tr1735;
	case 1674: goto tr1735;
	case 1675: goto tr1735;
	case 1676: goto tr1735;
	case 1677: goto tr1735;
	case 1678: goto tr1735;
	}
	}

	_out: {}
	}

#line 1531 "ext/dtext/dtext.cpp.rl"

  g_debug("EOF; closing stray blocks");
  dstack_close_all();
  g_debug("done");

  return output;
}
