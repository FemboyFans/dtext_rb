
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
static const int dtext_start = 1672;
static const int dtext_first_final = 1672;
static const int dtext_error = 0;

static const int dtext_en_basic_inline = 1698;
static const int dtext_en_inline = 1701;
static const int dtext_en_code = 1931;
static const int dtext_en_nodtext = 1935;
static const int dtext_en_table = 1939;
static const int dtext_en_main = 1672;


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
		case 1672: goto st1672;
		case 1673: goto st1673;
		case 1: goto st1;
		case 1674: goto st1674;
		case 2: goto st2;
		case 1675: goto st1675;
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
		case 1676: goto st1676;
		case 17: goto st17;
		case 18: goto st18;
		case 19: goto st19;
		case 20: goto st20;
		case 21: goto st21;
		case 22: goto st22;
		case 1677: goto st1677;
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
		case 1678: goto st1678;
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
		case 1679: goto st1679;
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
		case 1680: goto st1680;
		case 1681: goto st1681;
		case 75: goto st75;
		case 1682: goto st1682;
		case 1683: goto st1683;
		case 76: goto st76;
		case 0: goto st0;
		case 1684: goto st1684;
		case 77: goto st77;
		case 78: goto st78;
		case 79: goto st79;
		case 1685: goto st1685;
		case 1686: goto st1686;
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
		case 1687: goto st1687;
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
		case 1688: goto st1688;
		case 100: goto st100;
		case 101: goto st101;
		case 102: goto st102;
		case 103: goto st103;
		case 104: goto st104;
		case 105: goto st105;
		case 106: goto st106;
		case 1689: goto st1689;
		case 107: goto st107;
		case 1690: goto st1690;
		case 108: goto st108;
		case 109: goto st109;
		case 110: goto st110;
		case 111: goto st111;
		case 112: goto st112;
		case 113: goto st113;
		case 114: goto st114;
		case 115: goto st115;
		case 116: goto st116;
		case 1691: goto st1691;
		case 117: goto st117;
		case 1692: goto st1692;
		case 118: goto st118;
		case 119: goto st119;
		case 120: goto st120;
		case 121: goto st121;
		case 122: goto st122;
		case 123: goto st123;
		case 124: goto st124;
		case 1693: goto st1693;
		case 125: goto st125;
		case 126: goto st126;
		case 127: goto st127;
		case 128: goto st128;
		case 129: goto st129;
		case 130: goto st130;
		case 131: goto st131;
		case 132: goto st132;
		case 1694: goto st1694;
		case 133: goto st133;
		case 134: goto st134;
		case 135: goto st135;
		case 1695: goto st1695;
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
		case 1696: goto st1696;
		case 1697: goto st1697;
		case 1698: goto st1698;
		case 1699: goto st1699;
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
		case 193: goto st193;
		case 194: goto st194;
		case 195: goto st195;
		case 196: goto st196;
		case 197: goto st197;
		case 198: goto st198;
		case 1700: goto st1700;
		case 199: goto st199;
		case 200: goto st200;
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
		case 1701: goto st1701;
		case 1702: goto st1702;
		case 1703: goto st1703;
		case 214: goto st214;
		case 215: goto st215;
		case 216: goto st216;
		case 1704: goto st1704;
		case 1705: goto st1705;
		case 1706: goto st1706;
		case 217: goto st217;
		case 1707: goto st1707;
		case 218: goto st218;
		case 1708: goto st1708;
		case 219: goto st219;
		case 220: goto st220;
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
		case 1709: goto st1709;
		case 232: goto st232;
		case 233: goto st233;
		case 234: goto st234;
		case 235: goto st235;
		case 236: goto st236;
		case 237: goto st237;
		case 1710: goto st1710;
		case 238: goto st238;
		case 239: goto st239;
		case 240: goto st240;
		case 241: goto st241;
		case 242: goto st242;
		case 243: goto st243;
		case 244: goto st244;
		case 245: goto st245;
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
		case 1711: goto st1711;
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
		case 1712: goto st1712;
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
		case 1713: goto st1713;
		case 1714: goto st1714;
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
		case 1715: goto st1715;
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
		case 1716: goto st1716;
		case 395: goto st395;
		case 396: goto st396;
		case 397: goto st397;
		case 1717: goto st1717;
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
		case 1718: goto st1718;
		case 409: goto st409;
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
		case 1719: goto st1719;
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
		case 1720: goto st1720;
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
		case 454: goto st454;
		case 1721: goto st1721;
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
		case 1722: goto st1722;
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
		case 1723: goto st1723;
		case 513: goto st513;
		case 514: goto st514;
		case 515: goto st515;
		case 516: goto st516;
		case 517: goto st517;
		case 518: goto st518;
		case 519: goto st519;
		case 520: goto st520;
		case 521: goto st521;
		case 1724: goto st1724;
		case 1725: goto st1725;
		case 522: goto st522;
		case 523: goto st523;
		case 524: goto st524;
		case 525: goto st525;
		case 1726: goto st1726;
		case 1727: goto st1727;
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
		case 1728: goto st1728;
		case 1729: goto st1729;
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
		case 566: goto st566;
		case 567: goto st567;
		case 568: goto st568;
		case 569: goto st569;
		case 1730: goto st1730;
		case 1731: goto st1731;
		case 570: goto st570;
		case 571: goto st571;
		case 572: goto st572;
		case 573: goto st573;
		case 1732: goto st1732;
		case 1733: goto st1733;
		case 574: goto st574;
		case 575: goto st575;
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
		case 1734: goto st1734;
		case 1735: goto st1735;
		case 586: goto st586;
		case 1736: goto st1736;
		case 1737: goto st1737;
		case 587: goto st587;
		case 588: goto st588;
		case 589: goto st589;
		case 590: goto st590;
		case 591: goto st591;
		case 592: goto st592;
		case 593: goto st593;
		case 594: goto st594;
		case 1738: goto st1738;
		case 1739: goto st1739;
		case 595: goto st595;
		case 1740: goto st1740;
		case 596: goto st596;
		case 597: goto st597;
		case 598: goto st598;
		case 599: goto st599;
		case 600: goto st600;
		case 601: goto st601;
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
		case 1741: goto st1741;
		case 614: goto st614;
		case 615: goto st615;
		case 616: goto st616;
		case 617: goto st617;
		case 618: goto st618;
		case 619: goto st619;
		case 620: goto st620;
		case 621: goto st621;
		case 622: goto st622;
		case 1742: goto st1742;
		case 1743: goto st1743;
		case 1744: goto st1744;
		case 1745: goto st1745;
		case 1746: goto st1746;
		case 623: goto st623;
		case 624: goto st624;
		case 1747: goto st1747;
		case 1748: goto st1748;
		case 1749: goto st1749;
		case 1750: goto st1750;
		case 1751: goto st1751;
		case 1752: goto st1752;
		case 625: goto st625;
		case 626: goto st626;
		case 1753: goto st1753;
		case 627: goto st627;
		case 628: goto st628;
		case 629: goto st629;
		case 630: goto st630;
		case 631: goto st631;
		case 632: goto st632;
		case 633: goto st633;
		case 634: goto st634;
		case 635: goto st635;
		case 1754: goto st1754;
		case 1755: goto st1755;
		case 1756: goto st1756;
		case 1757: goto st1757;
		case 1758: goto st1758;
		case 636: goto st636;
		case 637: goto st637;
		case 638: goto st638;
		case 639: goto st639;
		case 640: goto st640;
		case 641: goto st641;
		case 642: goto st642;
		case 643: goto st643;
		case 644: goto st644;
		case 645: goto st645;
		case 1759: goto st1759;
		case 1760: goto st1760;
		case 1761: goto st1761;
		case 1762: goto st1762;
		case 646: goto st646;
		case 647: goto st647;
		case 1763: goto st1763;
		case 1764: goto st1764;
		case 1765: goto st1765;
		case 648: goto st648;
		case 649: goto st649;
		case 1766: goto st1766;
		case 1767: goto st1767;
		case 1768: goto st1768;
		case 1769: goto st1769;
		case 1770: goto st1770;
		case 1771: goto st1771;
		case 1772: goto st1772;
		case 1773: goto st1773;
		case 650: goto st650;
		case 651: goto st651;
		case 1774: goto st1774;
		case 1775: goto st1775;
		case 1776: goto st1776;
		case 652: goto st652;
		case 653: goto st653;
		case 1777: goto st1777;
		case 1778: goto st1778;
		case 1779: goto st1779;
		case 1780: goto st1780;
		case 1781: goto st1781;
		case 1782: goto st1782;
		case 654: goto st654;
		case 655: goto st655;
		case 1783: goto st1783;
		case 656: goto st656;
		case 1784: goto st1784;
		case 1785: goto st1785;
		case 1786: goto st1786;
		case 657: goto st657;
		case 658: goto st658;
		case 1787: goto st1787;
		case 1788: goto st1788;
		case 1789: goto st1789;
		case 1790: goto st1790;
		case 1791: goto st1791;
		case 659: goto st659;
		case 660: goto st660;
		case 1792: goto st1792;
		case 1793: goto st1793;
		case 1794: goto st1794;
		case 1795: goto st1795;
		case 1796: goto st1796;
		case 661: goto st661;
		case 662: goto st662;
		case 663: goto st663;
		case 1797: goto st1797;
		case 664: goto st664;
		case 665: goto st665;
		case 666: goto st666;
		case 667: goto st667;
		case 668: goto st668;
		case 669: goto st669;
		case 670: goto st670;
		case 671: goto st671;
		case 672: goto st672;
		case 673: goto st673;
		case 674: goto st674;
		case 1798: goto st1798;
		case 675: goto st675;
		case 676: goto st676;
		case 1799: goto st1799;
		case 1800: goto st1800;
		case 1801: goto st1801;
		case 1802: goto st1802;
		case 1803: goto st1803;
		case 1804: goto st1804;
		case 1805: goto st1805;
		case 1806: goto st1806;
		case 1807: goto st1807;
		case 677: goto st677;
		case 678: goto st678;
		case 679: goto st679;
		case 680: goto st680;
		case 681: goto st681;
		case 682: goto st682;
		case 683: goto st683;
		case 684: goto st684;
		case 685: goto st685;
		case 1808: goto st1808;
		case 686: goto st686;
		case 687: goto st687;
		case 688: goto st688;
		case 689: goto st689;
		case 690: goto st690;
		case 691: goto st691;
		case 692: goto st692;
		case 693: goto st693;
		case 694: goto st694;
		case 695: goto st695;
		case 1809: goto st1809;
		case 696: goto st696;
		case 697: goto st697;
		case 698: goto st698;
		case 699: goto st699;
		case 700: goto st700;
		case 701: goto st701;
		case 702: goto st702;
		case 703: goto st703;
		case 704: goto st704;
		case 705: goto st705;
		case 706: goto st706;
		case 1810: goto st1810;
		case 707: goto st707;
		case 708: goto st708;
		case 709: goto st709;
		case 710: goto st710;
		case 711: goto st711;
		case 712: goto st712;
		case 713: goto st713;
		case 714: goto st714;
		case 715: goto st715;
		case 716: goto st716;
		case 717: goto st717;
		case 718: goto st718;
		case 719: goto st719;
		case 1811: goto st1811;
		case 720: goto st720;
		case 721: goto st721;
		case 722: goto st722;
		case 723: goto st723;
		case 724: goto st724;
		case 725: goto st725;
		case 726: goto st726;
		case 727: goto st727;
		case 728: goto st728;
		case 729: goto st729;
		case 1812: goto st1812;
		case 1813: goto st1813;
		case 1814: goto st1814;
		case 1815: goto st1815;
		case 1816: goto st1816;
		case 1817: goto st1817;
		case 1818: goto st1818;
		case 1819: goto st1819;
		case 1820: goto st1820;
		case 1821: goto st1821;
		case 1822: goto st1822;
		case 1823: goto st1823;
		case 1824: goto st1824;
		case 730: goto st730;
		case 731: goto st731;
		case 1825: goto st1825;
		case 1826: goto st1826;
		case 1827: goto st1827;
		case 1828: goto st1828;
		case 1829: goto st1829;
		case 732: goto st732;
		case 733: goto st733;
		case 1830: goto st1830;
		case 1831: goto st1831;
		case 1832: goto st1832;
		case 1833: goto st1833;
		case 734: goto st734;
		case 735: goto st735;
		case 736: goto st736;
		case 737: goto st737;
		case 738: goto st738;
		case 739: goto st739;
		case 740: goto st740;
		case 741: goto st741;
		case 742: goto st742;
		case 1834: goto st1834;
		case 1835: goto st1835;
		case 1836: goto st1836;
		case 1837: goto st1837;
		case 1838: goto st1838;
		case 743: goto st743;
		case 744: goto st744;
		case 1839: goto st1839;
		case 1840: goto st1840;
		case 1841: goto st1841;
		case 1842: goto st1842;
		case 1843: goto st1843;
		case 745: goto st745;
		case 746: goto st746;
		case 1844: goto st1844;
		case 1845: goto st1845;
		case 1846: goto st1846;
		case 747: goto st747;
		case 748: goto st748;
		case 1847: goto st1847;
		case 749: goto st749;
		case 750: goto st750;
		case 751: goto st751;
		case 752: goto st752;
		case 753: goto st753;
		case 754: goto st754;
		case 755: goto st755;
		case 756: goto st756;
		case 757: goto st757;
		case 1848: goto st1848;
		case 1849: goto st1849;
		case 1850: goto st1850;
		case 1851: goto st1851;
		case 758: goto st758;
		case 759: goto st759;
		case 1852: goto st1852;
		case 1853: goto st1853;
		case 1854: goto st1854;
		case 1855: goto st1855;
		case 1856: goto st1856;
		case 1857: goto st1857;
		case 1858: goto st1858;
		case 760: goto st760;
		case 761: goto st761;
		case 1859: goto st1859;
		case 1860: goto st1860;
		case 1861: goto st1861;
		case 1862: goto st1862;
		case 762: goto st762;
		case 763: goto st763;
		case 1863: goto st1863;
		case 1864: goto st1864;
		case 1865: goto st1865;
		case 1866: goto st1866;
		case 1867: goto st1867;
		case 764: goto st764;
		case 765: goto st765;
		case 766: goto st766;
		case 767: goto st767;
		case 768: goto st768;
		case 769: goto st769;
		case 770: goto st770;
		case 1868: goto st1868;
		case 771: goto st771;
		case 772: goto st772;
		case 773: goto st773;
		case 774: goto st774;
		case 775: goto st775;
		case 776: goto st776;
		case 777: goto st777;
		case 778: goto st778;
		case 1869: goto st1869;
		case 1870: goto st1870;
		case 1871: goto st1871;
		case 1872: goto st1872;
		case 1873: goto st1873;
		case 1874: goto st1874;
		case 1875: goto st1875;
		case 1876: goto st1876;
		case 779: goto st779;
		case 780: goto st780;
		case 1877: goto st1877;
		case 1878: goto st1878;
		case 1879: goto st1879;
		case 1880: goto st1880;
		case 1881: goto st1881;
		case 1882: goto st1882;
		case 781: goto st781;
		case 782: goto st782;
		case 1883: goto st1883;
		case 1884: goto st1884;
		case 1885: goto st1885;
		case 1886: goto st1886;
		case 1887: goto st1887;
		case 1888: goto st1888;
		case 1889: goto st1889;
		case 1890: goto st1890;
		case 1891: goto st1891;
		case 783: goto st783;
		case 784: goto st784;
		case 1892: goto st1892;
		case 1893: goto st1893;
		case 1894: goto st1894;
		case 1895: goto st1895;
		case 1896: goto st1896;
		case 785: goto st785;
		case 786: goto st786;
		case 787: goto st787;
		case 1897: goto st1897;
		case 788: goto st788;
		case 789: goto st789;
		case 790: goto st790;
		case 791: goto st791;
		case 792: goto st792;
		case 793: goto st793;
		case 794: goto st794;
		case 795: goto st795;
		case 796: goto st796;
		case 1898: goto st1898;
		case 797: goto st797;
		case 798: goto st798;
		case 799: goto st799;
		case 800: goto st800;
		case 801: goto st801;
		case 802: goto st802;
		case 1899: goto st1899;
		case 1900: goto st1900;
		case 1901: goto st1901;
		case 1902: goto st1902;
		case 1903: goto st1903;
		case 1904: goto st1904;
		case 1905: goto st1905;
		case 1906: goto st1906;
		case 1907: goto st1907;
		case 1908: goto st1908;
		case 1909: goto st1909;
		case 1910: goto st1910;
		case 1911: goto st1911;
		case 803: goto st803;
		case 804: goto st804;
		case 805: goto st805;
		case 806: goto st806;
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
		case 821: goto st821;
		case 822: goto st822;
		case 823: goto st823;
		case 824: goto st824;
		case 825: goto st825;
		case 826: goto st826;
		case 827: goto st827;
		case 1912: goto st1912;
		case 828: goto st828;
		case 829: goto st829;
		case 1913: goto st1913;
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
		case 1914: goto st1914;
		case 895: goto st895;
		case 1915: goto st1915;
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
		case 1916: goto st1916;
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
		case 1917: goto st1917;
		case 1074: goto st1074;
		case 1075: goto st1075;
		case 1918: goto st1918;
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
		case 1919: goto st1919;
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
		case 1920: goto st1920;
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
		case 1154: goto st1154;
		case 1155: goto st1155;
		case 1156: goto st1156;
		case 1157: goto st1157;
		case 1158: goto st1158;
		case 1921: goto st1921;
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
		case 1181: goto st1181;
		case 1922: goto st1922;
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
		case 1923: goto st1923;
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
		case 1924: goto st1924;
		case 1925: goto st1925;
		case 1299: goto st1299;
		case 1300: goto st1300;
		case 1301: goto st1301;
		case 1302: goto st1302;
		case 1303: goto st1303;
		case 1304: goto st1304;
		case 1305: goto st1305;
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
		case 1926: goto st1926;
		case 1345: goto st1345;
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
		case 1927: goto st1927;
		case 1382: goto st1382;
		case 1383: goto st1383;
		case 1928: goto st1928;
		case 1384: goto st1384;
		case 1385: goto st1385;
		case 1386: goto st1386;
		case 1387: goto st1387;
		case 1929: goto st1929;
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
		case 1930: goto st1930;
		case 1398: goto st1398;
		case 1399: goto st1399;
		case 1400: goto st1400;
		case 1401: goto st1401;
		case 1402: goto st1402;
		case 1403: goto st1403;
		case 1404: goto st1404;
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
		case 1931: goto st1931;
		case 1932: goto st1932;
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
		case 1933: goto st1933;
		case 1934: goto st1934;
		case 1935: goto st1935;
		case 1936: goto st1936;
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
		case 1937: goto st1937;
		case 1938: goto st1938;
		case 1939: goto st1939;
		case 1940: goto st1940;
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
		case 1941: goto st1941;
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
{( (stack.data()))[( top)++] = 1672;goto st1701;}}
  }
	break;
	default:
	{{( p) = ((( te)))-1;}}
	break;
	}
	}
	goto st1672;
tr3:
#line 809 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1672;goto st1701;}}
  }}
	goto st1672;
tr77:
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
{( (stack.data()))[( top)++] = 1672;goto st1939;}}
  }}
	goto st1672;
tr105:
#line 751 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    append_code_fence({ b1, b2 }, { a1, a2 });
  }}
	goto st1672;
tr130:
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
{( (stack.data()))[( top)++] = 1672;goto st1931;}}
  }}
	goto st1672;
tr131:
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
{( (stack.data()))[( top)++] = 1672;goto st1931;}}
  }}
	goto st1672;
tr133:
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
{( (stack.data()))[( top)++] = 1672;goto st1931;}}
  }}
	goto st1672;
tr134:
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
{( (stack.data()))[( top)++] = 1672;goto st1931;}}
  }}
	goto st1672;
tr157:
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
{( (stack.data()))[( top)++] = 1672;goto st1935;}}
  }}
	goto st1672;
tr158:
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
{( (stack.data()))[( top)++] = 1672;goto st1935;}}
  }}
	goto st1672;
tr169:
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
{( (stack.data()))[( top)++] = 1672;goto st1701;}}
  }}
	goto st1672;
tr2082:
#line 809 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1672;goto st1701;}}
  }}
	goto st1672;
tr2093:
#line 809 "ext/dtext/dtext.cpp.rl"
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
{( (stack.data()))[( top)++] = 1672;goto st1701;}}
  }}
	goto st1672;
tr2094:
#line 728 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_QUOTE);
  }}
	goto st1672;
tr2095:
#line 771 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_EXPAND);
  }}
	goto st1672;
tr2096:
#line 737 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_SPOILER);
  }}
	goto st1672;
tr2097:
#line 793 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    append_block("<hr>");
  }}
	goto st1672;
tr2098:
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
{( (stack.data()))[( top)++] = 1672;goto st1701;}}
  }}
	goto st1672;
tr2101:
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
{( (stack.data()))[( top)++] = 1672;goto st1701;}}
  }}
	goto st1672;
tr2110:
#line 723 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_QUOTE, "<blockquote>");
  }}
	goto st1672;
tr2111:
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
{( (stack.data()))[( top)++] = 1672;goto st1931;}}
  }}
	goto st1672;
tr2112:
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
{( (stack.data()))[( top)++] = 1672;goto st1931;}}
  }}
	goto st1672;
tr2113:
#line 762 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("block [expand=]");
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_EXPAND, "<details>");
    append_block("<summary>");
    append_block_html_escaped({ a1, a2 });
    append_block("</summary><div>");
  }}
	goto st1672;
tr2115:
#line 756 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_EXPAND, "<details>");
    append_block("<summary>Show</summary><div>");
  }}
	goto st1672;
tr2116:
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
{( (stack.data()))[( top)++] = 1672;goto st1935;}}
  }}
	goto st1672;
tr2117:
#line 732 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    dstack_open_element(BLOCK_SPOILER, "<div class=\"spoiler\">");
  }}
	goto st1672;
st1672:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1672;
case 1672:
#line 1 "NONE"
	{( ts) = ( p);}
#line 3184 "ext/dtext/dtext.cpp"
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
		case 9: goto tr2083;
		case 10: goto tr6;
		case 32: goto tr2083;
		case 42: goto tr2085;
		case 72: goto tr2087;
		case 96: goto tr2088;
		case 104: goto tr2087;
		case 3388: goto tr2089;
		case 3419: goto tr2090;
		case 3644: goto tr2091;
		case 3675: goto tr2092;
	}
	if ( _widec < 14 ) {
		if ( _widec > 8 ) {
			if ( 11 <= _widec && _widec <= 13 )
				goto tr2084;
		} else
			goto tr2082;
	} else if ( _widec > 59 ) {
		if ( _widec > 90 ) {
			if ( 92 <= _widec )
				goto tr2082;
		} else if ( _widec >= 61 )
			goto tr2082;
	} else
		goto tr2082;
	goto st0;
tr1:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 805 "ext/dtext/dtext.cpp.rl"
	{( act) = 147;}
	goto st1673;
tr4:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 804 "ext/dtext/dtext.cpp.rl"
	{( act) = 146;}
	goto st1673;
st1673:
	if ( ++( p) == ( pe) )
		goto _test_eof1673;
case 1673:
#line 3244 "ext/dtext/dtext.cpp"
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
tr2083:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1674;
st1674:
	if ( ++( p) == ( pe) )
		goto _test_eof1674;
case 1674:
#line 3273 "ext/dtext/dtext.cpp"
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
	goto tr2093;
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
#line 805 "ext/dtext/dtext.cpp.rl"
	{( act) = 147;}
	goto st1675;
tr6:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 804 "ext/dtext/dtext.cpp.rl"
	{( act) = 146;}
	goto st1675;
st1675:
	if ( ++( p) == ( pe) )
		goto _test_eof1675;
case 1675:
#line 3348 "ext/dtext/dtext.cpp"
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
		goto st1676;
	goto tr0;
st1676:
	if ( ++( p) == ( pe) )
		goto _test_eof1676;
case 1676:
	switch( (*( p)) ) {
		case 9: goto st1676;
		case 32: goto st1676;
	}
	goto tr2094;
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
		goto st1677;
	goto tr0;
st1677:
	if ( ++( p) == ( pe) )
		goto _test_eof1677;
case 1677:
	switch( (*( p)) ) {
		case 9: goto st1677;
		case 32: goto st1677;
	}
	goto tr2095;
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
		goto st1676;
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
		goto st1677;
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
		case 3646: goto st1678;
		case 3667: goto st45;
		case 3699: goto st45;
	}
	goto tr0;
st1678:
	if ( ++( p) == ( pe) )
		goto _test_eof1678;
case 1678:
	switch( (*( p)) ) {
		case 9: goto st1678;
		case 32: goto st1678;
	}
	goto tr2096;
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
		goto st1678;
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
		case 3677: goto st1678;
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
		goto st1678;
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
		case 0: goto st1679;
		case 9: goto st59;
		case 10: goto st1679;
		case 32: goto st59;
	}
	goto tr3;
st1679:
	if ( ++( p) == ( pe) )
		goto _test_eof1679;
case 1679:
	switch( (*( p)) ) {
		case 0: goto st1679;
		case 10: goto st1679;
	}
	goto tr2097;
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
tr2084:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1680;
st1680:
	if ( ++( p) == ( pe) )
		goto _test_eof1680;
case 1680:
#line 4429 "ext/dtext/dtext.cpp"
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
	goto tr2093;
tr2085:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1681;
st1681:
	if ( ++( p) == ( pe) )
		goto _test_eof1681;
case 1681:
#line 4464 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr87;
		case 32: goto tr87;
		case 42: goto st76;
	}
	goto tr2093;
tr87:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
	goto st75;
st75:
	if ( ++( p) == ( pe) )
		goto _test_eof75;
case 75:
#line 4479 "ext/dtext/dtext.cpp"
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
	goto st1682;
st1682:
	if ( ++( p) == ( pe) )
		goto _test_eof1682;
case 1682:
#line 4496 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2098;
		case 10: goto tr2098;
		case 13: goto tr2098;
	}
	goto st1682;
tr86:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1683;
st1683:
	if ( ++( p) == ( pe) )
		goto _test_eof1683;
case 1683:
#line 4511 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2098;
		case 9: goto tr86;
		case 10: goto tr2098;
		case 13: goto tr2098;
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
tr2087:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1684;
st1684:
	if ( ++( p) == ( pe) )
		goto _test_eof1684;
case 1684:
#line 4541 "ext/dtext/dtext.cpp"
	if ( 49 <= (*( p)) && (*( p)) <= 54 )
		goto tr2100;
	goto tr2093;
tr2100:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st77;
st77:
	if ( ++( p) == ( pe) )
		goto _test_eof77;
case 77:
#line 4553 "ext/dtext/dtext.cpp"
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
#line 4567 "ext/dtext/dtext.cpp"
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
#line 4592 "ext/dtext/dtext.cpp"
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
	goto st1685;
tr93:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1685;
st1685:
	if ( ++( p) == ( pe) )
		goto _test_eof1685;
case 1685:
#line 4625 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1685;
		case 32: goto st1685;
	}
	goto tr2101;
tr2088:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1686;
st1686:
	if ( ++( p) == ( pe) )
		goto _test_eof1686;
case 1686:
#line 4639 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 96 )
		goto st80;
	goto tr2093;
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
#line 4660 "ext/dtext/dtext.cpp"
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
#line 4690 "ext/dtext/dtext.cpp"
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
#line 4704 "ext/dtext/dtext.cpp"
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
#line 4724 "ext/dtext/dtext.cpp"
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
#line 4770 "ext/dtext/dtext.cpp"
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
#line 4794 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st82;
		case 9: goto st89;
		case 10: goto st82;
		case 32: goto st89;
	}
	goto tr3;
tr2089:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1687;
st1687:
	if ( ++( p) == ( pe) )
		goto _test_eof1687;
case 1687:
#line 4812 "ext/dtext/dtext.cpp"
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
	goto tr2093;
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
		goto st1688;
	goto tr3;
st1688:
	if ( ++( p) == ( pe) )
		goto _test_eof1688;
case 1688:
	if ( (*( p)) == 32 )
		goto st1688;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1688;
	goto tr2110;
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
#line 5003 "ext/dtext/dtext.cpp"
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
	goto st1689;
st1689:
	if ( ++( p) == ( pe) )
		goto _test_eof1689;
case 1689:
#line 5025 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr131;
		case 9: goto st107;
		case 10: goto tr131;
		case 32: goto st107;
	}
	goto tr2111;
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
	goto st1690;
st1690:
	if ( ++( p) == ( pe) )
		goto _test_eof1690;
case 1690:
#line 5052 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr134;
		case 9: goto st108;
		case 10: goto tr134;
		case 32: goto st108;
	}
	goto tr2112;
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
		case 62: goto st1692;
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
#line 5135 "ext/dtext/dtext.cpp"
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
#line 5154 "ext/dtext/dtext.cpp"
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
	goto st1691;
tr147:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1691;
st1691:
	if ( ++( p) == ( pe) )
		goto _test_eof1691;
case 1691:
#line 5176 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 32 )
		goto st1691;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1691;
	goto tr2113;
tr146:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st117;
st117:
	if ( ++( p) == ( pe) )
		goto _test_eof117;
case 117:
#line 5190 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr3;
		case 9: goto tr146;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr146;
		case 62: goto tr147;
	}
	goto tr144;
st1692:
	if ( ++( p) == ( pe) )
		goto _test_eof1692;
case 1692:
	if ( (*( p)) == 32 )
		goto st1692;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1692;
	goto tr2115;
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
	goto st1693;
st1693:
	if ( ++( p) == ( pe) )
		goto _test_eof1693;
case 1693:
#line 5278 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr158;
		case 9: goto st125;
		case 10: goto tr158;
		case 32: goto st125;
	}
	goto tr2116;
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
		case 62: goto st1694;
		case 83: goto st133;
		case 115: goto st133;
	}
	goto tr3;
st1694:
	if ( ++( p) == ( pe) )
		goto _test_eof1694;
case 1694:
	if ( (*( p)) == 32 )
		goto st1694;
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st1694;
	goto tr2117;
st133:
	if ( ++( p) == ( pe) )
		goto _test_eof133;
case 133:
	if ( (*( p)) == 62 )
		goto st1694;
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
tr2090:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1695;
st1695:
	if ( ++( p) == ( pe) )
		goto _test_eof1695;
case 1695:
#line 5405 "ext/dtext/dtext.cpp"
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
	goto tr2093;
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
#line 5497 "ext/dtext/dtext.cpp"
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
		case 93: goto st1692;
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
#line 5573 "ext/dtext/dtext.cpp"
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
#line 5592 "ext/dtext/dtext.cpp"
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
#line 5608 "ext/dtext/dtext.cpp"
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
		goto st1688;
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
		case 93: goto st1694;
		case 115: goto st171;
	}
	goto tr3;
st171:
	if ( ++( p) == ( pe) )
		goto _test_eof171;
case 171:
	if ( (*( p)) == 93 )
		goto st1694;
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
tr2091:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1696;
st1696:
	if ( ++( p) == ( pe) )
		goto _test_eof1696;
case 1696:
#line 5821 "ext/dtext/dtext.cpp"
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
	goto tr2093;
tr2092:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 809 "ext/dtext/dtext.cpp.rl"
	{( act) = 148;}
	goto st1697;
st1697:
	if ( ++( p) == ( pe) )
		goto _test_eof1697;
case 1697:
#line 5860 "ext/dtext/dtext.cpp"
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
	goto tr2093;
tr206:
#line 315 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_html_escaped((*( p))); }}
	goto st1698;
tr212:
#line 303 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_B, { ts, te }); }}
	goto st1698;
tr213:
#line 305 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_I, { ts, te }); }}
	goto st1698;
tr214:
#line 307 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_S, { ts, te }); }}
	goto st1698;
tr222:
#line 313 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_SUB, { ts, te }); }}
	goto st1698;
tr223:
#line 311 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_SUP, { ts, te }); }}
	goto st1698;
tr224:
#line 309 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_U, { ts, te }); }}
	goto st1698;
tr225:
#line 302 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_B, "<strong>"); }}
	goto st1698;
tr227:
#line 304 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_I, "<em>"); }}
	goto st1698;
tr228:
#line 306 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_S, "<s>"); }}
	goto st1698;
tr237:
#line 312 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_SUB, "<sub>"); }}
	goto st1698;
tr238:
#line 310 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_SUP, "<sup>"); }}
	goto st1698;
tr239:
#line 308 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_U, "<u>"); }}
	goto st1698;
tr2124:
#line 315 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append_html_escaped((*( p))); }}
	goto st1698;
tr2125:
#line 314 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1698;
tr2128:
#line 315 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_html_escaped((*( p))); }}
	goto st1698;
st1698:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1698;
case 1698:
#line 1 "NONE"
	{( ts) = ( p);}
#line 5959 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2125;
		case 60: goto tr2126;
		case 91: goto tr2127;
	}
	goto tr2124;
tr2126:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1699;
st1699:
	if ( ++( p) == ( pe) )
		goto _test_eof1699;
case 1699:
#line 5974 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st174;
		case 66: goto st187;
		case 69: goto st188;
		case 73: goto st189;
		case 83: goto st190;
		case 85: goto st198;
		case 98: goto st187;
		case 101: goto st188;
		case 105: goto st189;
		case 115: goto st190;
		case 117: goto st198;
	}
	goto tr2128;
st174:
	if ( ++( p) == ( pe) )
		goto _test_eof174;
case 174:
	switch( (*( p)) ) {
		case 66: goto st175;
		case 69: goto st176;
		case 73: goto st177;
		case 83: goto st178;
		case 85: goto st186;
		case 98: goto st175;
		case 101: goto st176;
		case 105: goto st177;
		case 115: goto st178;
		case 117: goto st186;
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
		case 85: goto st183;
		case 116: goto st179;
		case 117: goto st183;
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
	switch( (*( p)) ) {
		case 66: goto st184;
		case 80: goto st185;
		case 98: goto st184;
		case 112: goto st185;
	}
	goto tr206;
st184:
	if ( ++( p) == ( pe) )
		goto _test_eof184;
case 184:
	if ( (*( p)) == 62 )
		goto tr222;
	goto tr206;
st185:
	if ( ++( p) == ( pe) )
		goto _test_eof185;
case 185:
	if ( (*( p)) == 62 )
		goto tr223;
	goto tr206;
st186:
	if ( ++( p) == ( pe) )
		goto _test_eof186;
case 186:
	if ( (*( p)) == 62 )
		goto tr224;
	goto tr206;
st187:
	if ( ++( p) == ( pe) )
		goto _test_eof187;
case 187:
	if ( (*( p)) == 62 )
		goto tr225;
	goto tr206;
st188:
	if ( ++( p) == ( pe) )
		goto _test_eof188;
case 188:
	switch( (*( p)) ) {
		case 77: goto st189;
		case 109: goto st189;
	}
	goto tr206;
st189:
	if ( ++( p) == ( pe) )
		goto _test_eof189;
case 189:
	if ( (*( p)) == 62 )
		goto tr227;
	goto tr206;
st190:
	if ( ++( p) == ( pe) )
		goto _test_eof190;
case 190:
	switch( (*( p)) ) {
		case 62: goto tr228;
		case 84: goto st191;
		case 85: goto st195;
		case 116: goto st191;
		case 117: goto st195;
	}
	goto tr206;
st191:
	if ( ++( p) == ( pe) )
		goto _test_eof191;
case 191:
	switch( (*( p)) ) {
		case 82: goto st192;
		case 114: goto st192;
	}
	goto tr206;
st192:
	if ( ++( p) == ( pe) )
		goto _test_eof192;
case 192:
	switch( (*( p)) ) {
		case 79: goto st193;
		case 111: goto st193;
	}
	goto tr206;
st193:
	if ( ++( p) == ( pe) )
		goto _test_eof193;
case 193:
	switch( (*( p)) ) {
		case 78: goto st194;
		case 110: goto st194;
	}
	goto tr206;
st194:
	if ( ++( p) == ( pe) )
		goto _test_eof194;
case 194:
	switch( (*( p)) ) {
		case 71: goto st187;
		case 103: goto st187;
	}
	goto tr206;
st195:
	if ( ++( p) == ( pe) )
		goto _test_eof195;
case 195:
	switch( (*( p)) ) {
		case 66: goto st196;
		case 80: goto st197;
		case 98: goto st196;
		case 112: goto st197;
	}
	goto tr206;
st196:
	if ( ++( p) == ( pe) )
		goto _test_eof196;
case 196:
	if ( (*( p)) == 62 )
		goto tr237;
	goto tr206;
st197:
	if ( ++( p) == ( pe) )
		goto _test_eof197;
case 197:
	if ( (*( p)) == 62 )
		goto tr238;
	goto tr206;
st198:
	if ( ++( p) == ( pe) )
		goto _test_eof198;
case 198:
	if ( (*( p)) == 62 )
		goto tr239;
	goto tr206;
tr2127:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1700;
st1700:
	if ( ++( p) == ( pe) )
		goto _test_eof1700;
case 1700:
#line 6220 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st199;
		case 66: goto st207;
		case 73: goto st208;
		case 83: goto st209;
		case 85: goto st213;
		case 98: goto st207;
		case 105: goto st208;
		case 115: goto st209;
		case 117: goto st213;
	}
	goto tr2128;
st199:
	if ( ++( p) == ( pe) )
		goto _test_eof199;
case 199:
	switch( (*( p)) ) {
		case 66: goto st200;
		case 73: goto st201;
		case 83: goto st202;
		case 85: goto st206;
		case 98: goto st200;
		case 105: goto st201;
		case 115: goto st202;
		case 117: goto st206;
	}
	goto tr206;
st200:
	if ( ++( p) == ( pe) )
		goto _test_eof200;
case 200:
	if ( (*( p)) == 93 )
		goto tr212;
	goto tr206;
st201:
	if ( ++( p) == ( pe) )
		goto _test_eof201;
case 201:
	if ( (*( p)) == 93 )
		goto tr213;
	goto tr206;
st202:
	if ( ++( p) == ( pe) )
		goto _test_eof202;
case 202:
	switch( (*( p)) ) {
		case 85: goto st203;
		case 93: goto tr214;
		case 117: goto st203;
	}
	goto tr206;
st203:
	if ( ++( p) == ( pe) )
		goto _test_eof203;
case 203:
	switch( (*( p)) ) {
		case 66: goto st204;
		case 80: goto st205;
		case 98: goto st204;
		case 112: goto st205;
	}
	goto tr206;
st204:
	if ( ++( p) == ( pe) )
		goto _test_eof204;
case 204:
	if ( (*( p)) == 93 )
		goto tr222;
	goto tr206;
st205:
	if ( ++( p) == ( pe) )
		goto _test_eof205;
case 205:
	if ( (*( p)) == 93 )
		goto tr223;
	goto tr206;
st206:
	if ( ++( p) == ( pe) )
		goto _test_eof206;
case 206:
	if ( (*( p)) == 93 )
		goto tr224;
	goto tr206;
st207:
	if ( ++( p) == ( pe) )
		goto _test_eof207;
case 207:
	if ( (*( p)) == 93 )
		goto tr225;
	goto tr206;
st208:
	if ( ++( p) == ( pe) )
		goto _test_eof208;
case 208:
	if ( (*( p)) == 93 )
		goto tr227;
	goto tr206;
st209:
	if ( ++( p) == ( pe) )
		goto _test_eof209;
case 209:
	switch( (*( p)) ) {
		case 85: goto st210;
		case 93: goto tr228;
		case 117: goto st210;
	}
	goto tr206;
st210:
	if ( ++( p) == ( pe) )
		goto _test_eof210;
case 210:
	switch( (*( p)) ) {
		case 66: goto st211;
		case 80: goto st212;
		case 98: goto st211;
		case 112: goto st212;
	}
	goto tr206;
st211:
	if ( ++( p) == ( pe) )
		goto _test_eof211;
case 211:
	if ( (*( p)) == 93 )
		goto tr237;
	goto tr206;
st212:
	if ( ++( p) == ( pe) )
		goto _test_eof212;
case 212:
	if ( (*( p)) == 93 )
		goto tr238;
	goto tr206;
st213:
	if ( ++( p) == ( pe) )
		goto _test_eof213;
case 213:
	if ( (*( p)) == 93 )
		goto tr239;
	goto tr206;
tr250:
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
{( (stack.data()))[( top)++] = 1701;goto st1931;}}
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
	goto st1701;
tr253:
#line 625 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append({ ts, te });
  }}
	goto st1701;
tr257:
#line 629 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1701;
tr259:
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
	goto st1701;
tr294:
#line 565 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TD, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1701;
tr295:
#line 559 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TH, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1701;
tr304:
#line 548 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( a1))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1701;
tr316:
#line 508 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1701;
tr342:
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
	goto st1701;
tr346:
#line 445 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    g_debug("inline [/tn]");

    if (dstack_check(INLINE_TN)) {
      dstack_close_element(INLINE_TN, { ts, te });
    } else if (dstack_close_element(BLOCK_TN, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1701;
tr367:
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
	goto st1701;
tr374:
#line 537 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1701;
tr377:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 537 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1701;
tr388:
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
	goto st1701;
tr470:
#line 394 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_bare_named_url({ b1, b2 + 1 }, { a1, a2 });
  }}
	goto st1701;
tr536:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 398 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ b1, b2 }, { a1, a2 });
  }}
	goto st1701;
tr546:
#line 608 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("'"); }}
	goto st1701;
tr551:
#line 604 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&amp;"); }}
	goto st1701;
tr554:
#line 609 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("'"); }}
	goto st1701;
tr556:
#line 612 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('*'); }}
	goto st1701;
tr562:
#line 613 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append(':'); }}
	goto st1701;
tr566:
#line 614 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('@'); }}
	goto st1701;
tr572:
#line 615 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('`'); }}
	goto st1701;
tr573:
#line 606 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&gt;"); }}
	goto st1701;
tr581:
#line 610 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('{'); }}
	goto st1701;
tr582:
#line 611 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('['); }}
	goto st1701;
tr583:
#line 605 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&lt;"); }}
	goto st1701;
tr586:
#line 616 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('#'); }}
	goto st1701;
tr592:
#line 617 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append('.'); }}
	goto st1701;
tr596:
#line 607 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ append("&quot;"); }}
	goto st1701;
tr822:
#line 345 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_id_link("dmail", "dmail", "/dmails/", { a1, a2 }); }}
	goto st1701;
tr843:
#line 342 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{ append_id_link("topic", "forum-topic", "/forum_topics/", { a1, a2 }); }}
	goto st1701;
tr869:
#line 410 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_bare_unnamed_url({ ts, te });
  }}
	goto st1701;
tr986:
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ g1, g2 }, { f1, f2 });
  }}
	goto st1701;
tr1002:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 402 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
	goto st1701;
tr1004:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 402 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
	goto st1701;
tr1026:
#line 429 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_B, { ts, te }); }}
	goto st1701;
tr1042:
#line 431 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_I, { ts, te }); }}
	goto st1701;
tr1054:
#line 433 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_S, { ts, te }); }}
	goto st1701;
tr1076:
#line 435 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_U, { ts, te }); }}
	goto st1701;
tr1078:
#line 428 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_B, "<strong>"); }}
	goto st1701;
tr1079:
#line 455 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (header_mode) {
      append_html_escaped("<br>");
    } else {
      append("<br>");
    };
  }}
	goto st1701;
tr1090:
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
{( (stack.data()))[( top)++] = 1701;goto st1931;}}
  }}
	goto st1701;
tr1091:
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
{( (stack.data()))[( top)++] = 1701;goto st1931;}}
  }}
	goto st1701;
tr1093:
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
{( (stack.data()))[( top)++] = 1701;goto st1931;}}
  }}
	goto st1701;
tr1094:
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
{( (stack.data()))[( top)++] = 1701;goto st1931;}}
  }}
	goto st1701;
tr1288:
#line 430 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_I, "<em>"); }}
	goto st1701;
tr1296:
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
{( (stack.data()))[( top)++] = 1701;goto st1935;}}
  }}
	goto st1701;
tr1297:
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
{( (stack.data()))[( top)++] = 1701;goto st1935;}}
  }}
	goto st1701;
tr1305:
#line 432 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_S, "<s>"); }}
	goto st1701;
tr1312:
#line 514 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element(INLINE_SPOILER, "<span class=\"spoiler\">");
  }}
	goto st1701;
tr1326:
#line 441 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element(INLINE_TN, "<span class=\"tn\">");
  }}
	goto st1701;
tr1328:
#line 434 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_U, "<u>"); }}
	goto st1701;
tr1356:
#line 398 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ b1, b2 }, { a1, a2 });
  }}
	goto st1701;
tr1460:
#line 414 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_unnamed_url({ a1, a2 });
  }}
	goto st1701;
tr1587:
#line 439 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_SUB, { ts, te }); }}
	goto st1701;
tr1588:
#line 437 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_close_element(INLINE_SUP, { ts, te }); }}
	goto st1701;
tr1606:
#line 402 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_named_url({ a1, a2 }, { b1, b2 });
  }}
	goto st1701;
tr1632:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 414 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_unnamed_url({ a1, a2 });
  }}
	goto st1701;
tr1654:
#line 438 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_SUB, "<sub>"); }}
	goto st1701;
tr1655:
#line 436 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{ dstack_open_element(INLINE_SUP, "<sup>"); }}
	goto st1701;
tr1659:
#line 418 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_mention({ a1, a2 + 1 });
  }}
	goto st1701;
tr2138:
#line 629 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1701;
tr2145:
#line 619 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append(' ');
  }}
	goto st1701;
tr2169:
#line 629 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1701;
tr2170:
#line 625 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append({ ts, te });
  }}
	goto st1701;
tr2172:
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
	goto st1701;
tr2179:
#line 543 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_QUOTE);
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1701;
tr2180:
#line 554 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_until(BLOCK_EXPAND);
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1701;
tr2181:
#line 537 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    dstack_close_leaf_blocks();
    {( p) = (( ts))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1701;
tr2182:
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
	goto st1701;
tr2185:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 422 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    g_debug("inline list");
    {( p) = (( ts + 1))-1;}
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1701;
tr2189:
#line 394 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_bare_named_url({ b1, b2 + 1 }, { a1, a2 });
  }}
	goto st1701;
tr2201:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 386 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1701;
tr2203:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 386 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1701;
tr2205:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 390 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1701;
tr2207:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 390 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_wiki_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1701;
tr2211:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 378 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_internal_anchor_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1701;
tr2213:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 378 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_internal_anchor_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { e1, e2 });
  }}
	goto st1701;
tr2215:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 382 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_internal_anchor_link({ a1, a2 }, { b1, b2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1701;
tr2217:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
#line 382 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_internal_anchor_link({ a1, a2 }, { b1, b2 }, { d1, d2 }, { e1, e2 });
  }}
	goto st1701;
tr2219:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 374 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 });
  }}
	goto st1701;
tr2221:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 374 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { c1, c2 }, { d1, d2 });
  }}
	goto st1701;
tr2223:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 370 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { d1, d2 });
  }}
	goto st1701;
tr2225:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 370 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_post_search_link({ a1, a2 }, { b1, b2 }, { b1, b2 }, { d1, d2 });
  }}
	goto st1701;
tr2237:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 353 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("alias", "tag-alias", "/tags/aliases/", { a1, a2 }); }}
	goto st1701;
tr2244:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 349 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("artist", "artist", "/artists/", { a1, a2 }); }}
	goto st1701;
tr2246:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 350 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("artist changes", "artist-changes-for", "/artists/versions?search[artist_id]=", { a1, a2 }); }}
	goto st1701;
tr2252:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 363 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("avoid posting", "avoid-posting", "/avoid_postings/", { a1, a2 }); }}
	goto st1701;
tr2258:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 351 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("ban", "ban", "/bans/", { a1, a2 }); }}
	goto st1701;
tr2262:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 352 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("BUR", "bulk-update-request", "/bulk_update_requests/", { a1, a2 }); }}
	goto st1701;
tr2272:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 344 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("comment", "comment", "/comments/", { a1, a2 }); }}
	goto st1701;
tr2276:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 367 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("commit", "github-commit", "https://github.com/PawsMovin/PawsMovin/commit/", { a1, a2 }); }}
	goto st1701;
tr2284:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 345 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("dmail", "dmail", "/dmails/", { a1, a2 }); }}
	goto st1701;
tr2287:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 346 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_dmail_key_link({ a1, a2 }, { b1, b2 }); }}
	goto st1701;
tr2291:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 362 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("avoid posting", "avoid-posting", "/avoid_postings/", { a1, a2 }); }}
	goto st1701;
tr2298:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 339 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("flag", "post-flag", "/posts/flags/", { a1, a2 }); }}
	goto st1701;
tr2305:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 341 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("forum", "forum-post", "/forum_posts/", { a1, a2 }); }}
	goto st1701;
tr2307:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 342 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("topic", "forum-topic", "/forum_topics/", { a1, a2 }); }}
	goto st1701;
tr2310:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 343 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_paged_link("topic #", { a1, a2 }, "<a class=\"dtext-link dtext-id-link dtext-forum-topic-id-link\" href=\"", "/forum_topics/", "?page=", { b1, b2 }); }}
	goto st1701;
tr2320:
#line 410 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_bare_unnamed_url({ ts, te });
  }}
	goto st1701;
tr2333:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 354 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("implication", "tag-implication", "/tags/implications/", { a1, a2 }); }}
	goto st1701;
tr2339:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 365 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("issue", "github", "https://github.com/PawsMovin/PawsMovin/issues/", { a1, a2 }); }}
	goto st1701;
tr2344:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 355 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("mod action", "mod-action", "/mod_actions/", { a1, a2 }); }}
	goto st1701;
tr2350:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 340 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("note", "note", "/notes/", { a1, a2 }); }}
	goto st1701;
tr2358:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 347 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("pool", "pool", "/pools/", { a1, a2 }); }}
	goto st1701;
tr2362:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 337 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("post", "post", "/posts/", { a1, a2 }); }}
	goto st1701;
tr2364:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 338 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("post changes", "post-changes-for", "/posts/versions?search[post_id]=", { a1, a2 }); }}
	goto st1701;
tr2369:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 366 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("pull", "github-pull", "https://github.com/PawsMovin/PawsMovin/pull/", { a1, a2 }); }}
	goto st1701;
tr2377:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 356 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("record", "user-feedback", "/users/feedbacks/", { a1, a2 }); }}
	goto st1701;
tr2382:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 359 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("set", "set", "/post_sets/", { a1, a2 }); }}
	goto st1701;
tr2392:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 361 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("takedown", "takedown", "/takedowns/", { a1, a2 }); }}
	goto st1701;
tr2401:
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
	goto st1701;
tr2408:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 360 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("ticket", "ticket", "/tickets/", { a1, a2 }); }}
	goto st1701;
tr2417:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 348 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("user", "user", "/users/", { a1, a2 }); }}
	goto st1701;
tr2425:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 357 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("wiki", "wiki-page", "/wiki_pages/", { a1, a2 }); }}
	goto st1701;
tr2427:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 358 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{ append_id_link("wiki changes", "wiki-page-changes-for", "/wiki_pages/versions?search[wiki_page_id]=", { a1, a2 }); }}
	goto st1701;
tr2453:
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
{( (stack.data()))[( top)++] = 1701;goto st1931;}}
  }}
	goto st1701;
tr2454:
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
{( (stack.data()))[( top)++] = 1701;goto st1931;}}
  }}
	goto st1701;
tr2455:
#line 406 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_named_url({ g1, g2 }, { f1, f2 });
  }}
	goto st1701;
tr2456:
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
{( (stack.data()))[( top)++] = 1701;goto st1935;}}
  }}
	goto st1701;
tr2476:
#line 418 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_mention({ a1, a2 + 1 });
  }}
	goto st1701;
st1701:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1701;
case 1701:
#line 1 "NONE"
	{( ts) = ( p);}
#line 7464 "ext/dtext/dtext.cpp"
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
		case 0: goto tr2142;
		case 9: goto tr2143;
		case 10: goto tr2144;
		case 13: goto tr2145;
		case 32: goto tr2143;
		case 34: goto tr2146;
		case 38: goto tr2147;
		case 65: goto tr2149;
		case 66: goto tr2150;
		case 67: goto tr2151;
		case 68: goto tr2152;
		case 70: goto tr2153;
		case 72: goto tr2154;
		case 73: goto tr2155;
		case 77: goto tr2156;
		case 78: goto tr2157;
		case 80: goto tr2158;
		case 82: goto tr2159;
		case 83: goto tr2160;
		case 84: goto tr2161;
		case 85: goto tr2162;
		case 87: goto tr2163;
		case 91: goto tr2164;
		case 97: goto tr2149;
		case 98: goto tr2150;
		case 99: goto tr2151;
		case 100: goto tr2152;
		case 102: goto tr2153;
		case 104: goto tr2154;
		case 105: goto tr2155;
		case 109: goto tr2156;
		case 110: goto tr2157;
		case 112: goto tr2158;
		case 114: goto tr2159;
		case 115: goto tr2160;
		case 116: goto tr2161;
		case 117: goto tr2162;
		case 119: goto tr2163;
		case 123: goto tr2165;
		case 828: goto tr2166;
		case 1084: goto tr2167;
		case 1344: goto tr2138;
		case 1600: goto tr2138;
		case 1856: goto tr2138;
		case 2112: goto tr2168;
	}
	if ( _widec < 48 ) {
		if ( _widec < -32 ) {
			if ( _widec > -63 ) {
				if ( -62 <= _widec && _widec <= -33 )
					goto st1702;
			} else
				goto tr2138;
		} else if ( _widec > -17 ) {
			if ( _widec > -12 ) {
				if ( -11 <= _widec && _widec <= 47 )
					goto tr2138;
			} else if ( _widec >= -16 )
				goto tr2141;
		} else
			goto tr2140;
	} else if ( _widec > 57 ) {
		if ( _widec < 69 ) {
			if ( _widec > 59 ) {
				if ( 61 <= _widec && _widec <= 63 )
					goto tr2138;
			} else if ( _widec >= 58 )
				goto tr2138;
		} else if ( _widec > 90 ) {
			if ( _widec < 101 ) {
				if ( 92 <= _widec && _widec <= 96 )
					goto tr2138;
			} else if ( _widec > 122 ) {
				if ( 124 <= _widec )
					goto tr2138;
			} else
				goto tr2148;
		} else
			goto tr2148;
	} else
		goto tr2148;
	goto st0;
st1702:
	if ( ++( p) == ( pe) )
		goto _test_eof1702;
case 1702:
	if ( (*( p)) <= -65 )
		goto tr251;
	goto tr2169;
tr251:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1703;
st1703:
	if ( ++( p) == ( pe) )
		goto _test_eof1703;
case 1703:
#line 7582 "ext/dtext/dtext.cpp"
	if ( (*( p)) < -32 ) {
		if ( -62 <= (*( p)) && (*( p)) <= -33 )
			goto st214;
	} else if ( (*( p)) > -17 ) {
		if ( -16 <= (*( p)) && (*( p)) <= -12 )
			goto st216;
	} else
		goto st215;
	goto tr2170;
st214:
	if ( ++( p) == ( pe) )
		goto _test_eof214;
case 214:
	if ( (*( p)) <= -65 )
		goto tr251;
	goto tr250;
st215:
	if ( ++( p) == ( pe) )
		goto _test_eof215;
case 215:
	if ( (*( p)) <= -65 )
		goto st214;
	goto tr250;
st216:
	if ( ++( p) == ( pe) )
		goto _test_eof216;
case 216:
	if ( (*( p)) <= -65 )
		goto st215;
	goto tr253;
tr2140:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1704;
st1704:
	if ( ++( p) == ( pe) )
		goto _test_eof1704;
case 1704:
#line 7623 "ext/dtext/dtext.cpp"
	if ( (*( p)) <= -65 )
		goto st214;
	goto tr2169;
tr2141:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1705;
st1705:
	if ( ++( p) == ( pe) )
		goto _test_eof1705;
case 1705:
#line 7637 "ext/dtext/dtext.cpp"
	if ( (*( p)) <= -65 )
		goto st215;
	goto tr2169;
tr255:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 571 "ext/dtext/dtext.cpp.rl"
	{( act) = 91;}
	goto st1706;
tr2142:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 623 "ext/dtext/dtext.cpp.rl"
	{( act) = 108;}
	goto st1706;
st1706:
	if ( ++( p) == ( pe) )
		goto _test_eof1706;
case 1706:
#line 7657 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr255;
		case 9: goto st217;
		case 10: goto tr255;
		case 32: goto st217;
	}
	goto tr250;
st217:
	if ( ++( p) == ( pe) )
		goto _test_eof217;
case 217:
	switch( (*( p)) ) {
		case 0: goto tr255;
		case 9: goto st217;
		case 10: goto tr255;
		case 32: goto st217;
	}
	goto tr250;
tr2143:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1707;
st1707:
	if ( ++( p) == ( pe) )
		goto _test_eof1707;
case 1707:
#line 7686 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st217;
		case 9: goto st218;
		case 10: goto st217;
		case 32: goto st218;
	}
	goto tr2169;
st218:
	if ( ++( p) == ( pe) )
		goto _test_eof218;
case 218:
	switch( (*( p)) ) {
		case 0: goto st217;
		case 9: goto st218;
		case 10: goto st217;
		case 32: goto st218;
	}
	goto tr257;
tr2144:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 589 "ext/dtext/dtext.cpp.rl"
	{( act) = 92;}
	goto st1708;
st1708:
	if ( ++( p) == ( pe) )
		goto _test_eof1708;
case 1708:
#line 7715 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr255;
		case 9: goto st219;
		case 10: goto tr2173;
		case 32: goto st219;
		case 42: goto tr2174;
		case 60: goto tr2175;
		case 72: goto st342;
		case 91: goto tr2177;
		case 96: goto st384;
		case 104: goto st342;
	}
	goto tr2172;
st219:
	if ( ++( p) == ( pe) )
		goto _test_eof219;
case 219:
	switch( (*( p)) ) {
		case 0: goto tr255;
		case 9: goto st219;
		case 10: goto tr255;
		case 32: goto st219;
		case 60: goto tr261;
		case 91: goto tr262;
	}
	goto tr259;
tr261:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st220;
st220:
	if ( ++( p) == ( pe) )
		goto _test_eof220;
case 220:
#line 7750 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st221;
		case 66: goto st246;
		case 72: goto st256;
		case 81: goto st251;
		case 83: goto st259;
		case 98: goto st246;
		case 104: goto st256;
		case 113: goto st251;
		case 115: goto st259;
	}
	goto tr259;
st221:
	if ( ++( p) == ( pe) )
		goto _test_eof221;
case 221:
	switch( (*( p)) ) {
		case 66: goto st222;
		case 69: goto st232;
		case 81: goto st238;
		case 84: goto st243;
		case 98: goto st222;
		case 101: goto st232;
		case 113: goto st238;
		case 116: goto st243;
	}
	goto tr259;
st222:
	if ( ++( p) == ( pe) )
		goto _test_eof222;
case 222:
	switch( (*( p)) ) {
		case 76: goto st223;
		case 108: goto st223;
	}
	goto tr259;
st223:
	if ( ++( p) == ( pe) )
		goto _test_eof223;
case 223:
	switch( (*( p)) ) {
		case 79: goto st224;
		case 111: goto st224;
	}
	goto tr250;
st224:
	if ( ++( p) == ( pe) )
		goto _test_eof224;
case 224:
	switch( (*( p)) ) {
		case 67: goto st225;
		case 99: goto st225;
	}
	goto tr250;
st225:
	if ( ++( p) == ( pe) )
		goto _test_eof225;
case 225:
	switch( (*( p)) ) {
		case 75: goto st226;
		case 107: goto st226;
	}
	goto tr250;
st226:
	if ( ++( p) == ( pe) )
		goto _test_eof226;
case 226:
	switch( (*( p)) ) {
		case 81: goto st227;
		case 113: goto st227;
	}
	goto tr250;
st227:
	if ( ++( p) == ( pe) )
		goto _test_eof227;
case 227:
	switch( (*( p)) ) {
		case 85: goto st228;
		case 117: goto st228;
	}
	goto tr250;
st228:
	if ( ++( p) == ( pe) )
		goto _test_eof228;
case 228:
	switch( (*( p)) ) {
		case 79: goto st229;
		case 111: goto st229;
	}
	goto tr250;
st229:
	if ( ++( p) == ( pe) )
		goto _test_eof229;
case 229:
	switch( (*( p)) ) {
		case 84: goto st230;
		case 116: goto st230;
	}
	goto tr250;
st230:
	if ( ++( p) == ( pe) )
		goto _test_eof230;
case 230:
	switch( (*( p)) ) {
		case 69: goto st231;
		case 101: goto st231;
	}
	goto tr250;
st231:
	if ( ++( p) == ( pe) )
		goto _test_eof231;
case 231:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2653 )
		goto st1709;
	goto tr250;
st1709:
	if ( ++( p) == ( pe) )
		goto _test_eof1709;
case 1709:
	switch( (*( p)) ) {
		case 9: goto st1709;
		case 32: goto st1709;
	}
	goto tr2179;
st232:
	if ( ++( p) == ( pe) )
		goto _test_eof232;
case 232:
	switch( (*( p)) ) {
		case 88: goto st233;
		case 120: goto st233;
	}
	goto tr259;
st233:
	if ( ++( p) == ( pe) )
		goto _test_eof233;
case 233:
	switch( (*( p)) ) {
		case 80: goto st234;
		case 112: goto st234;
	}
	goto tr250;
st234:
	if ( ++( p) == ( pe) )
		goto _test_eof234;
case 234:
	switch( (*( p)) ) {
		case 65: goto st235;
		case 97: goto st235;
	}
	goto tr250;
st235:
	if ( ++( p) == ( pe) )
		goto _test_eof235;
case 235:
	switch( (*( p)) ) {
		case 78: goto st236;
		case 110: goto st236;
	}
	goto tr250;
st236:
	if ( ++( p) == ( pe) )
		goto _test_eof236;
case 236:
	switch( (*( p)) ) {
		case 68: goto st237;
		case 100: goto st237;
	}
	goto tr250;
st237:
	if ( ++( p) == ( pe) )
		goto _test_eof237;
case 237:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_EXPAND)  ) _widec += 256;
	}
	if ( _widec == 3134 )
		goto st1710;
	goto tr250;
st1710:
	if ( ++( p) == ( pe) )
		goto _test_eof1710;
case 1710:
	switch( (*( p)) ) {
		case 9: goto st1710;
		case 32: goto st1710;
	}
	goto tr2180;
st238:
	if ( ++( p) == ( pe) )
		goto _test_eof238;
case 238:
	switch( (*( p)) ) {
		case 85: goto st239;
		case 117: goto st239;
	}
	goto tr250;
st239:
	if ( ++( p) == ( pe) )
		goto _test_eof239;
case 239:
	switch( (*( p)) ) {
		case 79: goto st240;
		case 111: goto st240;
	}
	goto tr250;
st240:
	if ( ++( p) == ( pe) )
		goto _test_eof240;
case 240:
	switch( (*( p)) ) {
		case 84: goto st241;
		case 116: goto st241;
	}
	goto tr250;
st241:
	if ( ++( p) == ( pe) )
		goto _test_eof241;
case 241:
	switch( (*( p)) ) {
		case 69: goto st242;
		case 101: goto st242;
	}
	goto tr250;
st242:
	if ( ++( p) == ( pe) )
		goto _test_eof242;
case 242:
	_widec = (*( p));
	if ( 62 <= (*( p)) && (*( p)) <= 62 ) {
		_widec = (short)(2176 + ((*( p)) - -128));
		if ( 
#line 90 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 2622 )
		goto st1709;
	goto tr250;
st243:
	if ( ++( p) == ( pe) )
		goto _test_eof243;
case 243:
	switch( (*( p)) ) {
		case 68: goto st244;
		case 72: goto st245;
		case 100: goto st244;
		case 104: goto st245;
	}
	goto tr259;
st244:
	if ( ++( p) == ( pe) )
		goto _test_eof244;
case 244:
	if ( (*( p)) == 62 )
		goto tr294;
	goto tr250;
st245:
	if ( ++( p) == ( pe) )
		goto _test_eof245;
case 245:
	if ( (*( p)) == 62 )
		goto tr295;
	goto tr250;
st246:
	if ( ++( p) == ( pe) )
		goto _test_eof246;
case 246:
	switch( (*( p)) ) {
		case 76: goto st247;
		case 108: goto st247;
	}
	goto tr259;
st247:
	if ( ++( p) == ( pe) )
		goto _test_eof247;
case 247:
	switch( (*( p)) ) {
		case 79: goto st248;
		case 111: goto st248;
	}
	goto tr250;
st248:
	if ( ++( p) == ( pe) )
		goto _test_eof248;
case 248:
	switch( (*( p)) ) {
		case 67: goto st249;
		case 99: goto st249;
	}
	goto tr250;
st249:
	if ( ++( p) == ( pe) )
		goto _test_eof249;
case 249:
	switch( (*( p)) ) {
		case 75: goto st250;
		case 107: goto st250;
	}
	goto tr250;
st250:
	if ( ++( p) == ( pe) )
		goto _test_eof250;
case 250:
	switch( (*( p)) ) {
		case 81: goto st251;
		case 113: goto st251;
	}
	goto tr250;
st251:
	if ( ++( p) == ( pe) )
		goto _test_eof251;
case 251:
	switch( (*( p)) ) {
		case 85: goto st252;
		case 117: goto st252;
	}
	goto tr250;
st252:
	if ( ++( p) == ( pe) )
		goto _test_eof252;
case 252:
	switch( (*( p)) ) {
		case 79: goto st253;
		case 111: goto st253;
	}
	goto tr250;
st253:
	if ( ++( p) == ( pe) )
		goto _test_eof253;
case 253:
	switch( (*( p)) ) {
		case 84: goto st254;
		case 116: goto st254;
	}
	goto tr250;
st254:
	if ( ++( p) == ( pe) )
		goto _test_eof254;
case 254:
	switch( (*( p)) ) {
		case 69: goto st255;
		case 101: goto st255;
	}
	goto tr250;
st255:
	if ( ++( p) == ( pe) )
		goto _test_eof255;
case 255:
	if ( (*( p)) == 62 )
		goto tr304;
	goto tr250;
st256:
	if ( ++( p) == ( pe) )
		goto _test_eof256;
case 256:
	switch( (*( p)) ) {
		case 82: goto st257;
		case 114: goto st257;
	}
	goto tr259;
st257:
	if ( ++( p) == ( pe) )
		goto _test_eof257;
case 257:
	if ( (*( p)) == 62 )
		goto st258;
	goto tr259;
st258:
	if ( ++( p) == ( pe) )
		goto _test_eof258;
case 258:
	switch( (*( p)) ) {
		case 0: goto st1711;
		case 9: goto st258;
		case 10: goto st1711;
		case 32: goto st258;
	}
	goto tr259;
st1711:
	if ( ++( p) == ( pe) )
		goto _test_eof1711;
case 1711:
	switch( (*( p)) ) {
		case 0: goto st1711;
		case 10: goto st1711;
	}
	goto tr2181;
st259:
	if ( ++( p) == ( pe) )
		goto _test_eof259;
case 259:
	switch( (*( p)) ) {
		case 80: goto st260;
		case 112: goto st260;
	}
	goto tr259;
st260:
	if ( ++( p) == ( pe) )
		goto _test_eof260;
case 260:
	switch( (*( p)) ) {
		case 79: goto st261;
		case 111: goto st261;
	}
	goto tr259;
st261:
	if ( ++( p) == ( pe) )
		goto _test_eof261;
case 261:
	switch( (*( p)) ) {
		case 73: goto st262;
		case 105: goto st262;
	}
	goto tr259;
st262:
	if ( ++( p) == ( pe) )
		goto _test_eof262;
case 262:
	switch( (*( p)) ) {
		case 76: goto st263;
		case 108: goto st263;
	}
	goto tr259;
st263:
	if ( ++( p) == ( pe) )
		goto _test_eof263;
case 263:
	switch( (*( p)) ) {
		case 69: goto st264;
		case 101: goto st264;
	}
	goto tr259;
st264:
	if ( ++( p) == ( pe) )
		goto _test_eof264;
case 264:
	switch( (*( p)) ) {
		case 82: goto st265;
		case 114: goto st265;
	}
	goto tr259;
st265:
	if ( ++( p) == ( pe) )
		goto _test_eof265;
case 265:
	switch( (*( p)) ) {
		case 62: goto st266;
		case 83: goto st267;
		case 115: goto st267;
	}
	goto tr259;
st266:
	if ( ++( p) == ( pe) )
		goto _test_eof266;
case 266:
	switch( (*( p)) ) {
		case 0: goto tr316;
		case 9: goto st266;
		case 10: goto tr316;
		case 32: goto st266;
	}
	goto tr259;
st267:
	if ( ++( p) == ( pe) )
		goto _test_eof267;
case 267:
	if ( (*( p)) == 62 )
		goto st266;
	goto tr259;
tr262:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st268;
st268:
	if ( ++( p) == ( pe) )
		goto _test_eof268;
case 268:
#line 8239 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st269;
		case 72: goto st279;
		case 81: goto st281;
		case 83: goto st286;
		case 104: goto st279;
		case 113: goto st281;
		case 115: goto st286;
	}
	goto tr259;
st269:
	if ( ++( p) == ( pe) )
		goto _test_eof269;
case 269:
	switch( (*( p)) ) {
		case 69: goto st270;
		case 81: goto st227;
		case 84: goto st276;
		case 101: goto st270;
		case 113: goto st227;
		case 116: goto st276;
	}
	goto tr259;
st270:
	if ( ++( p) == ( pe) )
		goto _test_eof270;
case 270:
	switch( (*( p)) ) {
		case 88: goto st271;
		case 120: goto st271;
	}
	goto tr259;
st271:
	if ( ++( p) == ( pe) )
		goto _test_eof271;
case 271:
	switch( (*( p)) ) {
		case 80: goto st272;
		case 112: goto st272;
	}
	goto tr259;
st272:
	if ( ++( p) == ( pe) )
		goto _test_eof272;
case 272:
	switch( (*( p)) ) {
		case 65: goto st273;
		case 97: goto st273;
	}
	goto tr259;
st273:
	if ( ++( p) == ( pe) )
		goto _test_eof273;
case 273:
	switch( (*( p)) ) {
		case 78: goto st274;
		case 110: goto st274;
	}
	goto tr259;
st274:
	if ( ++( p) == ( pe) )
		goto _test_eof274;
case 274:
	switch( (*( p)) ) {
		case 68: goto st275;
		case 100: goto st275;
	}
	goto tr259;
st275:
	if ( ++( p) == ( pe) )
		goto _test_eof275;
case 275:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_EXPAND)  ) _widec += 256;
	}
	if ( _widec == 3165 )
		goto st1710;
	goto tr259;
st276:
	if ( ++( p) == ( pe) )
		goto _test_eof276;
case 276:
	switch( (*( p)) ) {
		case 68: goto st277;
		case 72: goto st278;
		case 100: goto st277;
		case 104: goto st278;
	}
	goto tr259;
st277:
	if ( ++( p) == ( pe) )
		goto _test_eof277;
case 277:
	if ( (*( p)) == 93 )
		goto tr294;
	goto tr259;
st278:
	if ( ++( p) == ( pe) )
		goto _test_eof278;
case 278:
	if ( (*( p)) == 93 )
		goto tr295;
	goto tr259;
st279:
	if ( ++( p) == ( pe) )
		goto _test_eof279;
case 279:
	switch( (*( p)) ) {
		case 82: goto st280;
		case 114: goto st280;
	}
	goto tr259;
st280:
	if ( ++( p) == ( pe) )
		goto _test_eof280;
case 280:
	if ( (*( p)) == 93 )
		goto st258;
	goto tr259;
st281:
	if ( ++( p) == ( pe) )
		goto _test_eof281;
case 281:
	switch( (*( p)) ) {
		case 85: goto st282;
		case 117: goto st282;
	}
	goto tr259;
st282:
	if ( ++( p) == ( pe) )
		goto _test_eof282;
case 282:
	switch( (*( p)) ) {
		case 79: goto st283;
		case 111: goto st283;
	}
	goto tr259;
st283:
	if ( ++( p) == ( pe) )
		goto _test_eof283;
case 283:
	switch( (*( p)) ) {
		case 84: goto st284;
		case 116: goto st284;
	}
	goto tr259;
st284:
	if ( ++( p) == ( pe) )
		goto _test_eof284;
case 284:
	switch( (*( p)) ) {
		case 69: goto st285;
		case 101: goto st285;
	}
	goto tr259;
st285:
	if ( ++( p) == ( pe) )
		goto _test_eof285;
case 285:
	if ( (*( p)) == 93 )
		goto tr304;
	goto tr259;
st286:
	if ( ++( p) == ( pe) )
		goto _test_eof286;
case 286:
	switch( (*( p)) ) {
		case 80: goto st287;
		case 112: goto st287;
	}
	goto tr259;
st287:
	if ( ++( p) == ( pe) )
		goto _test_eof287;
case 287:
	switch( (*( p)) ) {
		case 79: goto st288;
		case 111: goto st288;
	}
	goto tr259;
st288:
	if ( ++( p) == ( pe) )
		goto _test_eof288;
case 288:
	switch( (*( p)) ) {
		case 73: goto st289;
		case 105: goto st289;
	}
	goto tr259;
st289:
	if ( ++( p) == ( pe) )
		goto _test_eof289;
case 289:
	switch( (*( p)) ) {
		case 76: goto st290;
		case 108: goto st290;
	}
	goto tr259;
st290:
	if ( ++( p) == ( pe) )
		goto _test_eof290;
case 290:
	switch( (*( p)) ) {
		case 69: goto st291;
		case 101: goto st291;
	}
	goto tr259;
st291:
	if ( ++( p) == ( pe) )
		goto _test_eof291;
case 291:
	switch( (*( p)) ) {
		case 82: goto st292;
		case 114: goto st292;
	}
	goto tr259;
st292:
	if ( ++( p) == ( pe) )
		goto _test_eof292;
case 292:
	switch( (*( p)) ) {
		case 83: goto st293;
		case 93: goto st266;
		case 115: goto st293;
	}
	goto tr259;
st293:
	if ( ++( p) == ( pe) )
		goto _test_eof293;
case 293:
	if ( (*( p)) == 93 )
		goto st266;
	goto tr259;
tr2173:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 571 "ext/dtext/dtext.cpp.rl"
	{( act) = 91;}
	goto st1712;
st1712:
	if ( ++( p) == ( pe) )
		goto _test_eof1712;
case 1712:
#line 8487 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr255;
		case 9: goto st217;
		case 10: goto tr2173;
		case 32: goto st217;
		case 60: goto st294;
		case 91: goto st298;
	}
	goto tr2182;
st294:
	if ( ++( p) == ( pe) )
		goto _test_eof294;
case 294:
	if ( (*( p)) == 47 )
		goto st295;
	goto tr342;
st295:
	if ( ++( p) == ( pe) )
		goto _test_eof295;
case 295:
	switch( (*( p)) ) {
		case 84: goto st296;
		case 116: goto st296;
	}
	goto tr342;
st296:
	if ( ++( p) == ( pe) )
		goto _test_eof296;
case 296:
	switch( (*( p)) ) {
		case 78: goto st297;
		case 110: goto st297;
	}
	goto tr342;
st297:
	if ( ++( p) == ( pe) )
		goto _test_eof297;
case 297:
	if ( (*( p)) == 62 )
		goto tr346;
	goto tr250;
st298:
	if ( ++( p) == ( pe) )
		goto _test_eof298;
case 298:
	if ( (*( p)) == 47 )
		goto st299;
	goto tr342;
st299:
	if ( ++( p) == ( pe) )
		goto _test_eof299;
case 299:
	switch( (*( p)) ) {
		case 84: goto st300;
		case 116: goto st300;
	}
	goto tr342;
st300:
	if ( ++( p) == ( pe) )
		goto _test_eof300;
case 300:
	switch( (*( p)) ) {
		case 78: goto st301;
		case 110: goto st301;
	}
	goto tr342;
st301:
	if ( ++( p) == ( pe) )
		goto _test_eof301;
case 301:
	if ( (*( p)) == 93 )
		goto tr346;
	goto tr250;
tr2174:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st302;
st302:
	if ( ++( p) == ( pe) )
		goto _test_eof302;
case 302:
#line 8569 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr350;
		case 32: goto tr350;
		case 42: goto st302;
	}
	goto tr259;
tr350:
#line 82 "ext/dtext/dtext.cpp.rl"
	{ e2 = p; }
	goto st303;
st303:
	if ( ++( p) == ( pe) )
		goto _test_eof303;
case 303:
#line 8584 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto tr353;
		case 10: goto tr259;
		case 13: goto tr259;
		case 32: goto tr353;
	}
	goto tr352;
tr352:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1713;
st1713:
	if ( ++( p) == ( pe) )
		goto _test_eof1713;
case 1713:
#line 8601 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2185;
		case 10: goto tr2185;
		case 13: goto tr2185;
	}
	goto st1713;
tr353:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1714;
st1714:
	if ( ++( p) == ( pe) )
		goto _test_eof1714;
case 1714:
#line 8616 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2185;
		case 9: goto tr353;
		case 10: goto tr2185;
		case 13: goto tr2185;
		case 32: goto tr353;
	}
	goto tr352;
tr2175:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st304;
st304:
	if ( ++( p) == ( pe) )
		goto _test_eof304;
case 304:
#line 8633 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st305;
		case 66: goto st246;
		case 67: goto st315;
		case 69: goto st322;
		case 72: goto st256;
		case 78: goto st331;
		case 81: goto st251;
		case 83: goto st259;
		case 84: goto st338;
		case 98: goto st246;
		case 99: goto st315;
		case 101: goto st322;
		case 104: goto st256;
		case 110: goto st331;
		case 113: goto st251;
		case 115: goto st259;
		case 116: goto st338;
	}
	goto tr259;
st305:
	if ( ++( p) == ( pe) )
		goto _test_eof305;
case 305:
	switch( (*( p)) ) {
		case 66: goto st222;
		case 69: goto st232;
		case 81: goto st238;
		case 83: goto st306;
		case 84: goto st314;
		case 98: goto st222;
		case 101: goto st232;
		case 113: goto st238;
		case 115: goto st306;
		case 116: goto st314;
	}
	goto tr259;
st306:
	if ( ++( p) == ( pe) )
		goto _test_eof306;
case 306:
	switch( (*( p)) ) {
		case 80: goto st307;
		case 112: goto st307;
	}
	goto tr259;
st307:
	if ( ++( p) == ( pe) )
		goto _test_eof307;
case 307:
	switch( (*( p)) ) {
		case 79: goto st308;
		case 111: goto st308;
	}
	goto tr250;
st308:
	if ( ++( p) == ( pe) )
		goto _test_eof308;
case 308:
	switch( (*( p)) ) {
		case 73: goto st309;
		case 105: goto st309;
	}
	goto tr250;
st309:
	if ( ++( p) == ( pe) )
		goto _test_eof309;
case 309:
	switch( (*( p)) ) {
		case 76: goto st310;
		case 108: goto st310;
	}
	goto tr250;
st310:
	if ( ++( p) == ( pe) )
		goto _test_eof310;
case 310:
	switch( (*( p)) ) {
		case 69: goto st311;
		case 101: goto st311;
	}
	goto tr250;
st311:
	if ( ++( p) == ( pe) )
		goto _test_eof311;
case 311:
	switch( (*( p)) ) {
		case 82: goto st312;
		case 114: goto st312;
	}
	goto tr250;
st312:
	if ( ++( p) == ( pe) )
		goto _test_eof312;
case 312:
	switch( (*( p)) ) {
		case 62: goto tr367;
		case 83: goto st313;
		case 115: goto st313;
	}
	goto tr250;
st313:
	if ( ++( p) == ( pe) )
		goto _test_eof313;
case 313:
	if ( (*( p)) == 62 )
		goto tr367;
	goto tr250;
st314:
	if ( ++( p) == ( pe) )
		goto _test_eof314;
case 314:
	switch( (*( p)) ) {
		case 68: goto st244;
		case 72: goto st245;
		case 78: goto st297;
		case 100: goto st244;
		case 104: goto st245;
		case 110: goto st297;
	}
	goto tr250;
st315:
	if ( ++( p) == ( pe) )
		goto _test_eof315;
case 315:
	switch( (*( p)) ) {
		case 79: goto st316;
		case 111: goto st316;
	}
	goto tr259;
st316:
	if ( ++( p) == ( pe) )
		goto _test_eof316;
case 316:
	switch( (*( p)) ) {
		case 68: goto st317;
		case 100: goto st317;
	}
	goto tr259;
st317:
	if ( ++( p) == ( pe) )
		goto _test_eof317;
case 317:
	switch( (*( p)) ) {
		case 69: goto st318;
		case 101: goto st318;
	}
	goto tr259;
st318:
	if ( ++( p) == ( pe) )
		goto _test_eof318;
case 318:
	switch( (*( p)) ) {
		case 9: goto st319;
		case 32: goto st319;
		case 61: goto st320;
		case 62: goto tr374;
	}
	goto tr259;
st319:
	if ( ++( p) == ( pe) )
		goto _test_eof319;
case 319:
	switch( (*( p)) ) {
		case 9: goto st319;
		case 32: goto st319;
		case 61: goto st320;
	}
	goto tr259;
st320:
	if ( ++( p) == ( pe) )
		goto _test_eof320;
case 320:
	switch( (*( p)) ) {
		case 9: goto st320;
		case 32: goto st320;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr375;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr375;
	} else
		goto tr375;
	goto tr259;
tr375:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st321;
st321:
	if ( ++( p) == ( pe) )
		goto _test_eof321;
case 321:
#line 8828 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 62 )
		goto tr377;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st321;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st321;
	} else
		goto st321;
	goto tr259;
st322:
	if ( ++( p) == ( pe) )
		goto _test_eof322;
case 322:
	switch( (*( p)) ) {
		case 88: goto st323;
		case 120: goto st323;
	}
	goto tr259;
st323:
	if ( ++( p) == ( pe) )
		goto _test_eof323;
case 323:
	switch( (*( p)) ) {
		case 80: goto st324;
		case 112: goto st324;
	}
	goto tr259;
st324:
	if ( ++( p) == ( pe) )
		goto _test_eof324;
case 324:
	switch( (*( p)) ) {
		case 65: goto st325;
		case 97: goto st325;
	}
	goto tr259;
st325:
	if ( ++( p) == ( pe) )
		goto _test_eof325;
case 325:
	switch( (*( p)) ) {
		case 78: goto st326;
		case 110: goto st326;
	}
	goto tr259;
st326:
	if ( ++( p) == ( pe) )
		goto _test_eof326;
case 326:
	switch( (*( p)) ) {
		case 68: goto st327;
		case 100: goto st327;
	}
	goto tr259;
st327:
	if ( ++( p) == ( pe) )
		goto _test_eof327;
case 327:
	switch( (*( p)) ) {
		case 9: goto st328;
		case 32: goto st328;
		case 61: goto st330;
		case 62: goto tr374;
	}
	goto tr259;
tr386:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st328;
st328:
	if ( ++( p) == ( pe) )
		goto _test_eof328;
case 328:
#line 8904 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto tr386;
		case 10: goto tr259;
		case 13: goto tr259;
		case 32: goto tr386;
		case 61: goto tr387;
		case 62: goto tr388;
	}
	goto tr385;
tr385:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st329;
st329:
	if ( ++( p) == ( pe) )
		goto _test_eof329;
case 329:
#line 8923 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 10: goto tr259;
		case 13: goto tr259;
		case 62: goto tr377;
	}
	goto st329;
tr387:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st330;
st330:
	if ( ++( p) == ( pe) )
		goto _test_eof330;
case 330:
#line 8939 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto tr387;
		case 10: goto tr259;
		case 13: goto tr259;
		case 32: goto tr387;
		case 62: goto tr388;
	}
	goto tr385;
st331:
	if ( ++( p) == ( pe) )
		goto _test_eof331;
case 331:
	switch( (*( p)) ) {
		case 79: goto st332;
		case 111: goto st332;
	}
	goto tr259;
st332:
	if ( ++( p) == ( pe) )
		goto _test_eof332;
case 332:
	switch( (*( p)) ) {
		case 68: goto st333;
		case 100: goto st333;
	}
	goto tr259;
st333:
	if ( ++( p) == ( pe) )
		goto _test_eof333;
case 333:
	switch( (*( p)) ) {
		case 84: goto st334;
		case 116: goto st334;
	}
	goto tr259;
st334:
	if ( ++( p) == ( pe) )
		goto _test_eof334;
case 334:
	switch( (*( p)) ) {
		case 69: goto st335;
		case 101: goto st335;
	}
	goto tr259;
st335:
	if ( ++( p) == ( pe) )
		goto _test_eof335;
case 335:
	switch( (*( p)) ) {
		case 88: goto st336;
		case 120: goto st336;
	}
	goto tr259;
st336:
	if ( ++( p) == ( pe) )
		goto _test_eof336;
case 336:
	switch( (*( p)) ) {
		case 84: goto st337;
		case 116: goto st337;
	}
	goto tr259;
st337:
	if ( ++( p) == ( pe) )
		goto _test_eof337;
case 337:
	if ( (*( p)) == 62 )
		goto tr374;
	goto tr259;
st338:
	if ( ++( p) == ( pe) )
		goto _test_eof338;
case 338:
	switch( (*( p)) ) {
		case 65: goto st339;
		case 97: goto st339;
	}
	goto tr259;
st339:
	if ( ++( p) == ( pe) )
		goto _test_eof339;
case 339:
	switch( (*( p)) ) {
		case 66: goto st340;
		case 98: goto st340;
	}
	goto tr259;
st340:
	if ( ++( p) == ( pe) )
		goto _test_eof340;
case 340:
	switch( (*( p)) ) {
		case 76: goto st341;
		case 108: goto st341;
	}
	goto tr259;
st341:
	if ( ++( p) == ( pe) )
		goto _test_eof341;
case 341:
	switch( (*( p)) ) {
		case 69: goto st337;
		case 101: goto st337;
	}
	goto tr259;
st342:
	if ( ++( p) == ( pe) )
		goto _test_eof342;
case 342:
	if ( 49 <= (*( p)) && (*( p)) <= 54 )
		goto tr399;
	goto tr259;
tr399:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st343;
st343:
	if ( ++( p) == ( pe) )
		goto _test_eof343;
case 343:
#line 9061 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr400;
		case 46: goto tr401;
	}
	goto tr259;
tr400:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st344;
st344:
	if ( ++( p) == ( pe) )
		goto _test_eof344;
case 344:
#line 9075 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto tr402;
		case 35: goto tr402;
		case 38: goto tr402;
		case 45: goto tr402;
		case 95: goto tr402;
	}
	if ( (*( p)) < 65 ) {
		if ( 47 <= (*( p)) && (*( p)) <= 58 )
			goto tr402;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr402;
	} else
		goto tr402;
	goto tr259;
tr402:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st345;
st345:
	if ( ++( p) == ( pe) )
		goto _test_eof345;
case 345:
#line 9100 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 33: goto st345;
		case 35: goto st345;
		case 38: goto st345;
		case 46: goto tr404;
		case 95: goto st345;
	}
	if ( (*( p)) < 65 ) {
		if ( 45 <= (*( p)) && (*( p)) <= 58 )
			goto st345;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st345;
	} else
		goto st345;
	goto tr259;
tr401:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1715;
tr404:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1715;
st1715:
	if ( ++( p) == ( pe) )
		goto _test_eof1715;
case 1715:
#line 9133 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1715;
		case 32: goto st1715;
	}
	goto tr2181;
tr2177:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st346;
st346:
	if ( ++( p) == ( pe) )
		goto _test_eof346;
case 346:
#line 9147 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st347;
		case 67: goto st357;
		case 69: goto st364;
		case 72: goto st279;
		case 78: goto st373;
		case 81: goto st281;
		case 83: goto st286;
		case 84: goto st380;
		case 99: goto st357;
		case 101: goto st364;
		case 104: goto st279;
		case 110: goto st373;
		case 113: goto st281;
		case 115: goto st286;
		case 116: goto st380;
	}
	goto tr259;
st347:
	if ( ++( p) == ( pe) )
		goto _test_eof347;
case 347:
	switch( (*( p)) ) {
		case 69: goto st270;
		case 81: goto st227;
		case 83: goto st348;
		case 84: goto st356;
		case 101: goto st270;
		case 113: goto st227;
		case 115: goto st348;
		case 116: goto st356;
	}
	goto tr259;
st348:
	if ( ++( p) == ( pe) )
		goto _test_eof348;
case 348:
	switch( (*( p)) ) {
		case 80: goto st349;
		case 112: goto st349;
	}
	goto tr259;
st349:
	if ( ++( p) == ( pe) )
		goto _test_eof349;
case 349:
	switch( (*( p)) ) {
		case 79: goto st350;
		case 111: goto st350;
	}
	goto tr259;
st350:
	if ( ++( p) == ( pe) )
		goto _test_eof350;
case 350:
	switch( (*( p)) ) {
		case 73: goto st351;
		case 105: goto st351;
	}
	goto tr259;
st351:
	if ( ++( p) == ( pe) )
		goto _test_eof351;
case 351:
	switch( (*( p)) ) {
		case 76: goto st352;
		case 108: goto st352;
	}
	goto tr259;
st352:
	if ( ++( p) == ( pe) )
		goto _test_eof352;
case 352:
	switch( (*( p)) ) {
		case 69: goto st353;
		case 101: goto st353;
	}
	goto tr259;
st353:
	if ( ++( p) == ( pe) )
		goto _test_eof353;
case 353:
	switch( (*( p)) ) {
		case 82: goto st354;
		case 114: goto st354;
	}
	goto tr259;
st354:
	if ( ++( p) == ( pe) )
		goto _test_eof354;
case 354:
	switch( (*( p)) ) {
		case 83: goto st355;
		case 93: goto tr367;
		case 115: goto st355;
	}
	goto tr259;
st355:
	if ( ++( p) == ( pe) )
		goto _test_eof355;
case 355:
	if ( (*( p)) == 93 )
		goto tr367;
	goto tr259;
st356:
	if ( ++( p) == ( pe) )
		goto _test_eof356;
case 356:
	switch( (*( p)) ) {
		case 68: goto st277;
		case 72: goto st278;
		case 78: goto st301;
		case 100: goto st277;
		case 104: goto st278;
		case 110: goto st301;
	}
	goto tr259;
st357:
	if ( ++( p) == ( pe) )
		goto _test_eof357;
case 357:
	switch( (*( p)) ) {
		case 79: goto st358;
		case 111: goto st358;
	}
	goto tr259;
st358:
	if ( ++( p) == ( pe) )
		goto _test_eof358;
case 358:
	switch( (*( p)) ) {
		case 68: goto st359;
		case 100: goto st359;
	}
	goto tr259;
st359:
	if ( ++( p) == ( pe) )
		goto _test_eof359;
case 359:
	switch( (*( p)) ) {
		case 69: goto st360;
		case 101: goto st360;
	}
	goto tr259;
st360:
	if ( ++( p) == ( pe) )
		goto _test_eof360;
case 360:
	switch( (*( p)) ) {
		case 9: goto st361;
		case 32: goto st361;
		case 61: goto st362;
		case 93: goto tr374;
	}
	goto tr259;
st361:
	if ( ++( p) == ( pe) )
		goto _test_eof361;
case 361:
	switch( (*( p)) ) {
		case 9: goto st361;
		case 32: goto st361;
		case 61: goto st362;
	}
	goto tr259;
st362:
	if ( ++( p) == ( pe) )
		goto _test_eof362;
case 362:
	switch( (*( p)) ) {
		case 9: goto st362;
		case 32: goto st362;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr424;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr424;
	} else
		goto tr424;
	goto tr259;
tr424:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st363;
st363:
	if ( ++( p) == ( pe) )
		goto _test_eof363;
case 363:
#line 9338 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto tr377;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st363;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st363;
	} else
		goto st363;
	goto tr259;
st364:
	if ( ++( p) == ( pe) )
		goto _test_eof364;
case 364:
	switch( (*( p)) ) {
		case 88: goto st365;
		case 120: goto st365;
	}
	goto tr259;
st365:
	if ( ++( p) == ( pe) )
		goto _test_eof365;
case 365:
	switch( (*( p)) ) {
		case 80: goto st366;
		case 112: goto st366;
	}
	goto tr259;
st366:
	if ( ++( p) == ( pe) )
		goto _test_eof366;
case 366:
	switch( (*( p)) ) {
		case 65: goto st367;
		case 97: goto st367;
	}
	goto tr259;
st367:
	if ( ++( p) == ( pe) )
		goto _test_eof367;
case 367:
	switch( (*( p)) ) {
		case 78: goto st368;
		case 110: goto st368;
	}
	goto tr259;
st368:
	if ( ++( p) == ( pe) )
		goto _test_eof368;
case 368:
	switch( (*( p)) ) {
		case 68: goto st369;
		case 100: goto st369;
	}
	goto tr259;
st369:
	if ( ++( p) == ( pe) )
		goto _test_eof369;
case 369:
	switch( (*( p)) ) {
		case 9: goto st370;
		case 32: goto st370;
		case 61: goto st372;
		case 93: goto tr374;
	}
	goto tr259;
tr434:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st370;
st370:
	if ( ++( p) == ( pe) )
		goto _test_eof370;
case 370:
#line 9414 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto tr434;
		case 10: goto tr259;
		case 13: goto tr259;
		case 32: goto tr434;
		case 61: goto tr435;
		case 93: goto tr388;
	}
	goto tr433;
tr433:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st371;
st371:
	if ( ++( p) == ( pe) )
		goto _test_eof371;
case 371:
#line 9433 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 10: goto tr259;
		case 13: goto tr259;
		case 93: goto tr377;
	}
	goto st371;
tr435:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st372;
st372:
	if ( ++( p) == ( pe) )
		goto _test_eof372;
case 372:
#line 9449 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr259;
		case 9: goto tr435;
		case 10: goto tr259;
		case 13: goto tr259;
		case 32: goto tr435;
		case 93: goto tr388;
	}
	goto tr433;
st373:
	if ( ++( p) == ( pe) )
		goto _test_eof373;
case 373:
	switch( (*( p)) ) {
		case 79: goto st374;
		case 111: goto st374;
	}
	goto tr259;
st374:
	if ( ++( p) == ( pe) )
		goto _test_eof374;
case 374:
	switch( (*( p)) ) {
		case 68: goto st375;
		case 100: goto st375;
	}
	goto tr259;
st375:
	if ( ++( p) == ( pe) )
		goto _test_eof375;
case 375:
	switch( (*( p)) ) {
		case 84: goto st376;
		case 116: goto st376;
	}
	goto tr259;
st376:
	if ( ++( p) == ( pe) )
		goto _test_eof376;
case 376:
	switch( (*( p)) ) {
		case 69: goto st377;
		case 101: goto st377;
	}
	goto tr259;
st377:
	if ( ++( p) == ( pe) )
		goto _test_eof377;
case 377:
	switch( (*( p)) ) {
		case 88: goto st378;
		case 120: goto st378;
	}
	goto tr259;
st378:
	if ( ++( p) == ( pe) )
		goto _test_eof378;
case 378:
	switch( (*( p)) ) {
		case 84: goto st379;
		case 116: goto st379;
	}
	goto tr259;
st379:
	if ( ++( p) == ( pe) )
		goto _test_eof379;
case 379:
	if ( (*( p)) == 93 )
		goto tr374;
	goto tr259;
st380:
	if ( ++( p) == ( pe) )
		goto _test_eof380;
case 380:
	switch( (*( p)) ) {
		case 65: goto st381;
		case 97: goto st381;
	}
	goto tr259;
st381:
	if ( ++( p) == ( pe) )
		goto _test_eof381;
case 381:
	switch( (*( p)) ) {
		case 66: goto st382;
		case 98: goto st382;
	}
	goto tr259;
st382:
	if ( ++( p) == ( pe) )
		goto _test_eof382;
case 382:
	switch( (*( p)) ) {
		case 76: goto st383;
		case 108: goto st383;
	}
	goto tr259;
st383:
	if ( ++( p) == ( pe) )
		goto _test_eof383;
case 383:
	switch( (*( p)) ) {
		case 69: goto st379;
		case 101: goto st379;
	}
	goto tr259;
st384:
	if ( ++( p) == ( pe) )
		goto _test_eof384;
case 384:
	if ( (*( p)) == 96 )
		goto st385;
	goto tr259;
st385:
	if ( ++( p) == ( pe) )
		goto _test_eof385;
case 385:
	if ( (*( p)) == 96 )
		goto st386;
	goto tr259;
tr449:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st386;
st386:
	if ( ++( p) == ( pe) )
		goto _test_eof386;
case 386:
#line 9580 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr448;
		case 9: goto tr449;
		case 10: goto tr448;
		case 32: goto tr449;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr450;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr450;
	} else
		goto tr450;
	goto tr259;
tr458:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st387;
tr448:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st387;
st387:
	if ( ++( p) == ( pe) )
		goto _test_eof387;
case 387:
#line 9610 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr452;
		case 10: goto tr452;
	}
	goto tr451;
tr451:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st388;
st388:
	if ( ++( p) == ( pe) )
		goto _test_eof388;
case 388:
#line 9624 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr454;
		case 10: goto tr454;
	}
	goto st388;
tr454:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st389;
tr452:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st389;
st389:
	if ( ++( p) == ( pe) )
		goto _test_eof389;
case 389:
#line 9644 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr454;
		case 10: goto tr454;
		case 96: goto st390;
	}
	goto st388;
st390:
	if ( ++( p) == ( pe) )
		goto _test_eof390;
case 390:
	switch( (*( p)) ) {
		case 0: goto tr454;
		case 10: goto tr454;
		case 96: goto st391;
	}
	goto st388;
st391:
	if ( ++( p) == ( pe) )
		goto _test_eof391;
case 391:
	switch( (*( p)) ) {
		case 0: goto tr454;
		case 10: goto tr454;
		case 96: goto st392;
	}
	goto st388;
st392:
	if ( ++( p) == ( pe) )
		goto _test_eof392;
case 392:
	switch( (*( p)) ) {
		case 0: goto tr374;
		case 9: goto st392;
		case 10: goto tr374;
		case 32: goto st392;
	}
	goto st388;
tr450:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st393;
st393:
	if ( ++( p) == ( pe) )
		goto _test_eof393;
case 393:
#line 9690 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr458;
		case 9: goto tr459;
		case 10: goto tr458;
		case 32: goto tr459;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st393;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st393;
	} else
		goto st393;
	goto tr259;
tr459:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st394;
st394:
	if ( ++( p) == ( pe) )
		goto _test_eof394;
case 394:
#line 9714 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st387;
		case 9: goto st394;
		case 10: goto st387;
		case 32: goto st394;
	}
	goto tr259;
tr2146:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1716;
st1716:
	if ( ++( p) == ( pe) )
		goto _test_eof1716;
case 1716:
#line 9732 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 34 )
		goto tr2169;
	goto tr2188;
tr2188:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st395;
st395:
	if ( ++( p) == ( pe) )
		goto _test_eof395;
case 395:
#line 9744 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 34 )
		goto tr464;
	goto st395;
tr464:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st396;
st396:
	if ( ++( p) == ( pe) )
		goto _test_eof396;
case 396:
#line 9756 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 58 )
		goto st397;
	goto tr257;
st397:
	if ( ++( p) == ( pe) )
		goto _test_eof397;
case 397:
	switch( (*( p)) ) {
		case 35: goto tr466;
		case 47: goto tr467;
		case 72: goto tr468;
		case 91: goto st456;
		case 104: goto tr468;
	}
	goto tr257;
tr466:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1717;
tr471:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1717;
st1717:
	if ( ++( p) == ( pe) )
		goto _test_eof1717;
case 1717:
#line 9790 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st400;
		case -29: goto st402;
		case -17: goto st404;
		case 32: goto tr2189;
		case 34: goto st408;
		case 35: goto tr2189;
		case 39: goto st408;
		case 44: goto st408;
		case 46: goto st408;
		case 60: goto tr2189;
		case 62: goto tr2189;
		case 63: goto st408;
		case 91: goto tr2189;
		case 93: goto tr2189;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2189;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st399;
		} else
			goto st398;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2189;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st408;
		} else
			goto tr2189;
	} else
		goto st407;
	goto tr471;
st398:
	if ( ++( p) == ( pe) )
		goto _test_eof398;
case 398:
	if ( (*( p)) <= -65 )
		goto tr471;
	goto tr470;
st399:
	if ( ++( p) == ( pe) )
		goto _test_eof399;
case 399:
	if ( (*( p)) <= -65 )
		goto st398;
	goto tr470;
st400:
	if ( ++( p) == ( pe) )
		goto _test_eof400;
case 400:
	if ( (*( p)) == -99 )
		goto st401;
	if ( (*( p)) <= -65 )
		goto st398;
	goto tr470;
st401:
	if ( ++( p) == ( pe) )
		goto _test_eof401;
case 401:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr471;
	} else
		goto tr471;
	goto tr470;
st402:
	if ( ++( p) == ( pe) )
		goto _test_eof402;
case 402:
	if ( (*( p)) == -128 )
		goto st403;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st398;
	goto tr470;
st403:
	if ( ++( p) == ( pe) )
		goto _test_eof403;
case 403:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr471;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr471;
	} else
		goto tr471;
	goto tr470;
st404:
	if ( ++( p) == ( pe) )
		goto _test_eof404;
case 404:
	switch( (*( p)) ) {
		case -68: goto st405;
		case -67: goto st406;
	}
	if ( (*( p)) <= -65 )
		goto st398;
	goto tr470;
st405:
	if ( ++( p) == ( pe) )
		goto _test_eof405;
case 405:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr471;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr471;
	} else
		goto tr471;
	goto tr470;
st406:
	if ( ++( p) == ( pe) )
		goto _test_eof406;
case 406:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr471;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr471;
		} else if ( (*( p)) >= -95 )
			goto tr471;
	} else
		goto tr471;
	goto tr470;
st407:
	if ( ++( p) == ( pe) )
		goto _test_eof407;
case 407:
	if ( (*( p)) <= -65 )
		goto st399;
	goto tr470;
st408:
	if ( ++( p) == ( pe) )
		goto _test_eof408;
case 408:
	switch( (*( p)) ) {
		case -30: goto st400;
		case -29: goto st402;
		case -17: goto st404;
		case 32: goto tr470;
		case 34: goto st408;
		case 35: goto tr470;
		case 39: goto st408;
		case 44: goto st408;
		case 46: goto st408;
		case 60: goto tr470;
		case 62: goto tr470;
		case 63: goto st408;
		case 91: goto tr470;
		case 93: goto tr470;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr470;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st399;
		} else
			goto st398;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr470;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st408;
		} else
			goto tr470;
	} else
		goto st407;
	goto tr471;
tr467:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1718;
tr483:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1718;
st1718:
	if ( ++( p) == ( pe) )
		goto _test_eof1718;
case 1718:
#line 9993 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st411;
		case -29: goto st413;
		case -17: goto st415;
		case 32: goto tr2189;
		case 34: goto st419;
		case 35: goto tr471;
		case 39: goto st419;
		case 44: goto st419;
		case 46: goto st419;
		case 60: goto tr2189;
		case 62: goto tr2189;
		case 63: goto st420;
		case 91: goto tr2189;
		case 93: goto tr2189;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2189;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st410;
		} else
			goto st409;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2189;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st419;
		} else
			goto tr2189;
	} else
		goto st418;
	goto tr483;
st409:
	if ( ++( p) == ( pe) )
		goto _test_eof409;
case 409:
	if ( (*( p)) <= -65 )
		goto tr483;
	goto tr470;
st410:
	if ( ++( p) == ( pe) )
		goto _test_eof410;
case 410:
	if ( (*( p)) <= -65 )
		goto st409;
	goto tr470;
st411:
	if ( ++( p) == ( pe) )
		goto _test_eof411;
case 411:
	if ( (*( p)) == -99 )
		goto st412;
	if ( (*( p)) <= -65 )
		goto st409;
	goto tr470;
st412:
	if ( ++( p) == ( pe) )
		goto _test_eof412;
case 412:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr483;
	} else
		goto tr483;
	goto tr470;
st413:
	if ( ++( p) == ( pe) )
		goto _test_eof413;
case 413:
	if ( (*( p)) == -128 )
		goto st414;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st409;
	goto tr470;
st414:
	if ( ++( p) == ( pe) )
		goto _test_eof414;
case 414:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr483;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr483;
	} else
		goto tr483;
	goto tr470;
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
	goto tr470;
st416:
	if ( ++( p) == ( pe) )
		goto _test_eof416;
case 416:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr483;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr483;
	} else
		goto tr483;
	goto tr470;
st417:
	if ( ++( p) == ( pe) )
		goto _test_eof417;
case 417:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr483;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr483;
		} else if ( (*( p)) >= -95 )
			goto tr483;
	} else
		goto tr483;
	goto tr470;
st418:
	if ( ++( p) == ( pe) )
		goto _test_eof418;
case 418:
	if ( (*( p)) <= -65 )
		goto st410;
	goto tr470;
st419:
	if ( ++( p) == ( pe) )
		goto _test_eof419;
case 419:
	switch( (*( p)) ) {
		case -30: goto st411;
		case -29: goto st413;
		case -17: goto st415;
		case 32: goto tr470;
		case 34: goto st419;
		case 35: goto tr471;
		case 39: goto st419;
		case 44: goto st419;
		case 46: goto st419;
		case 60: goto tr470;
		case 62: goto tr470;
		case 63: goto st420;
		case 91: goto tr470;
		case 93: goto tr470;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr470;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st410;
		} else
			goto st409;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr470;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st419;
		} else
			goto tr470;
	} else
		goto st418;
	goto tr483;
st420:
	if ( ++( p) == ( pe) )
		goto _test_eof420;
case 420:
	switch( (*( p)) ) {
		case -30: goto st423;
		case -29: goto st425;
		case -17: goto st427;
		case 32: goto tr250;
		case 34: goto st420;
		case 35: goto tr471;
		case 39: goto st420;
		case 44: goto st420;
		case 46: goto st420;
		case 63: goto st420;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr250;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st422;
		} else
			goto st421;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr250;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st420;
		} else
			goto tr250;
	} else
		goto st430;
	goto tr502;
st421:
	if ( ++( p) == ( pe) )
		goto _test_eof421;
case 421:
	if ( (*( p)) <= -65 )
		goto tr502;
	goto tr250;
tr502:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1719;
st1719:
	if ( ++( p) == ( pe) )
		goto _test_eof1719;
case 1719:
#line 10230 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st423;
		case -29: goto st425;
		case -17: goto st427;
		case 32: goto tr2189;
		case 34: goto st420;
		case 35: goto tr471;
		case 39: goto st420;
		case 44: goto st420;
		case 46: goto st420;
		case 63: goto st420;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2189;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st422;
		} else
			goto st421;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2189;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st420;
		} else
			goto tr2189;
	} else
		goto st430;
	goto tr502;
st422:
	if ( ++( p) == ( pe) )
		goto _test_eof422;
case 422:
	if ( (*( p)) <= -65 )
		goto st421;
	goto tr250;
st423:
	if ( ++( p) == ( pe) )
		goto _test_eof423;
case 423:
	if ( (*( p)) == -99 )
		goto st424;
	if ( (*( p)) <= -65 )
		goto st421;
	goto tr250;
st424:
	if ( ++( p) == ( pe) )
		goto _test_eof424;
case 424:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr502;
	} else
		goto tr502;
	goto tr250;
st425:
	if ( ++( p) == ( pe) )
		goto _test_eof425;
case 425:
	if ( (*( p)) == -128 )
		goto st426;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st421;
	goto tr250;
st426:
	if ( ++( p) == ( pe) )
		goto _test_eof426;
case 426:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr502;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr502;
	} else
		goto tr502;
	goto tr250;
st427:
	if ( ++( p) == ( pe) )
		goto _test_eof427;
case 427:
	switch( (*( p)) ) {
		case -68: goto st428;
		case -67: goto st429;
	}
	if ( (*( p)) <= -65 )
		goto st421;
	goto tr250;
st428:
	if ( ++( p) == ( pe) )
		goto _test_eof428;
case 428:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr502;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr502;
	} else
		goto tr502;
	goto tr250;
st429:
	if ( ++( p) == ( pe) )
		goto _test_eof429;
case 429:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr502;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr502;
		} else if ( (*( p)) >= -95 )
			goto tr502;
	} else
		goto tr502;
	goto tr250;
st430:
	if ( ++( p) == ( pe) )
		goto _test_eof430;
case 430:
	if ( (*( p)) <= -65 )
		goto st422;
	goto tr250;
tr468:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st431;
st431:
	if ( ++( p) == ( pe) )
		goto _test_eof431;
case 431:
#line 10367 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st432;
		case 116: goto st432;
	}
	goto tr257;
st432:
	if ( ++( p) == ( pe) )
		goto _test_eof432;
case 432:
	switch( (*( p)) ) {
		case 84: goto st433;
		case 116: goto st433;
	}
	goto tr257;
st433:
	if ( ++( p) == ( pe) )
		goto _test_eof433;
case 433:
	switch( (*( p)) ) {
		case 80: goto st434;
		case 112: goto st434;
	}
	goto tr257;
st434:
	if ( ++( p) == ( pe) )
		goto _test_eof434;
case 434:
	switch( (*( p)) ) {
		case 58: goto st435;
		case 83: goto st455;
		case 115: goto st455;
	}
	goto tr257;
st435:
	if ( ++( p) == ( pe) )
		goto _test_eof435;
case 435:
	if ( (*( p)) == 47 )
		goto st436;
	goto tr257;
st436:
	if ( ++( p) == ( pe) )
		goto _test_eof436;
case 436:
	if ( (*( p)) == 47 )
		goto st437;
	goto tr257;
st437:
	if ( ++( p) == ( pe) )
		goto _test_eof437;
case 437:
	switch( (*( p)) ) {
		case 45: goto st439;
		case 95: goto st439;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st440;
		} else if ( (*( p)) >= -62 )
			goto st438;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st439;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st439;
		} else
			goto st439;
	} else
		goto st441;
	goto tr257;
st438:
	if ( ++( p) == ( pe) )
		goto _test_eof438;
case 438:
	if ( (*( p)) <= -65 )
		goto st439;
	goto tr257;
st439:
	if ( ++( p) == ( pe) )
		goto _test_eof439;
case 439:
	switch( (*( p)) ) {
		case 45: goto st439;
		case 46: goto st442;
		case 95: goto st439;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st440;
		} else if ( (*( p)) >= -62 )
			goto st438;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st439;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st439;
		} else
			goto st439;
	} else
		goto st441;
	goto tr257;
st440:
	if ( ++( p) == ( pe) )
		goto _test_eof440;
case 440:
	if ( (*( p)) <= -65 )
		goto st438;
	goto tr257;
st441:
	if ( ++( p) == ( pe) )
		goto _test_eof441;
case 441:
	if ( (*( p)) <= -65 )
		goto st440;
	goto tr257;
st442:
	if ( ++( p) == ( pe) )
		goto _test_eof442;
case 442:
	switch( (*( p)) ) {
		case -30: goto st445;
		case -29: goto st448;
		case -17: goto st450;
		case 45: goto tr525;
		case 95: goto tr525;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st444;
		} else if ( (*( p)) >= -62 )
			goto st443;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto tr525;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr525;
		} else
			goto tr525;
	} else
		goto st453;
	goto tr250;
st443:
	if ( ++( p) == ( pe) )
		goto _test_eof443;
case 443:
	if ( (*( p)) <= -65 )
		goto tr525;
	goto tr250;
tr525:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1720;
st1720:
	if ( ++( p) == ( pe) )
		goto _test_eof1720;
case 1720:
#line 10537 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st445;
		case -29: goto st448;
		case -17: goto st450;
		case 35: goto tr471;
		case 46: goto st442;
		case 47: goto tr483;
		case 58: goto st454;
		case 63: goto st420;
		case 95: goto tr525;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st444;
		} else if ( (*( p)) >= -62 )
			goto st443;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr525;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr525;
		} else
			goto tr525;
	} else
		goto st453;
	goto tr2189;
st444:
	if ( ++( p) == ( pe) )
		goto _test_eof444;
case 444:
	if ( (*( p)) <= -65 )
		goto st443;
	goto tr250;
st445:
	if ( ++( p) == ( pe) )
		goto _test_eof445;
case 445:
	if ( (*( p)) == -99 )
		goto st446;
	if ( (*( p)) <= -65 )
		goto st443;
	goto tr250;
st446:
	if ( ++( p) == ( pe) )
		goto _test_eof446;
case 446:
	if ( (*( p)) == -83 )
		goto st447;
	if ( (*( p)) <= -65 )
		goto tr525;
	goto tr250;
st447:
	if ( ++( p) == ( pe) )
		goto _test_eof447;
case 447:
	switch( (*( p)) ) {
		case -30: goto st445;
		case -29: goto st448;
		case -17: goto st450;
		case 35: goto tr471;
		case 46: goto st442;
		case 47: goto tr483;
		case 58: goto st454;
		case 63: goto st420;
		case 95: goto tr525;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st444;
		} else if ( (*( p)) >= -62 )
			goto st443;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr525;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr525;
		} else
			goto tr525;
	} else
		goto st453;
	goto tr250;
st448:
	if ( ++( p) == ( pe) )
		goto _test_eof448;
case 448:
	if ( (*( p)) == -128 )
		goto st449;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st443;
	goto tr250;
st449:
	if ( ++( p) == ( pe) )
		goto _test_eof449;
case 449:
	if ( (*( p)) < -120 ) {
		if ( (*( p)) > -126 ) {
			if ( -125 <= (*( p)) && (*( p)) <= -121 )
				goto tr525;
		} else
			goto st447;
	} else if ( (*( p)) > -111 ) {
		if ( (*( p)) < -108 ) {
			if ( -110 <= (*( p)) && (*( p)) <= -109 )
				goto tr525;
		} else if ( (*( p)) > -100 ) {
			if ( -99 <= (*( p)) && (*( p)) <= -65 )
				goto tr525;
		} else
			goto st447;
	} else
		goto st447;
	goto tr250;
st450:
	if ( ++( p) == ( pe) )
		goto _test_eof450;
case 450:
	switch( (*( p)) ) {
		case -68: goto st451;
		case -67: goto st452;
	}
	if ( (*( p)) <= -65 )
		goto st443;
	goto tr250;
st451:
	if ( ++( p) == ( pe) )
		goto _test_eof451;
case 451:
	switch( (*( p)) ) {
		case -119: goto st447;
		case -67: goto st447;
	}
	if ( (*( p)) <= -65 )
		goto tr525;
	goto tr250;
st452:
	if ( ++( p) == ( pe) )
		goto _test_eof452;
case 452:
	switch( (*( p)) ) {
		case -99: goto st447;
		case -96: goto st447;
		case -93: goto st447;
	}
	if ( (*( p)) <= -65 )
		goto tr525;
	goto tr250;
st453:
	if ( ++( p) == ( pe) )
		goto _test_eof453;
case 453:
	if ( (*( p)) <= -65 )
		goto st444;
	goto tr250;
st454:
	if ( ++( p) == ( pe) )
		goto _test_eof454;
case 454:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr532;
	goto tr250;
tr532:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 394 "ext/dtext/dtext.cpp.rl"
	{( act) = 52;}
	goto st1721;
st1721:
	if ( ++( p) == ( pe) )
		goto _test_eof1721;
case 1721:
#line 10716 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr471;
		case 47: goto tr483;
		case 63: goto st420;
	}
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr532;
	goto tr2189;
st455:
	if ( ++( p) == ( pe) )
		goto _test_eof455;
case 455:
	if ( (*( p)) == 58 )
		goto st435;
	goto tr257;
st456:
	if ( ++( p) == ( pe) )
		goto _test_eof456;
case 456:
	switch( (*( p)) ) {
		case 35: goto tr533;
		case 47: goto tr533;
		case 72: goto tr534;
		case 104: goto tr534;
	}
	goto tr257;
tr533:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st457;
st457:
	if ( ++( p) == ( pe) )
		goto _test_eof457;
case 457:
#line 10751 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 93: goto tr536;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st457;
tr534:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st458;
st458:
	if ( ++( p) == ( pe) )
		goto _test_eof458;
case 458:
#line 10768 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st459;
		case 116: goto st459;
	}
	goto tr257;
st459:
	if ( ++( p) == ( pe) )
		goto _test_eof459;
case 459:
	switch( (*( p)) ) {
		case 84: goto st460;
		case 116: goto st460;
	}
	goto tr257;
st460:
	if ( ++( p) == ( pe) )
		goto _test_eof460;
case 460:
	switch( (*( p)) ) {
		case 80: goto st461;
		case 112: goto st461;
	}
	goto tr257;
st461:
	if ( ++( p) == ( pe) )
		goto _test_eof461;
case 461:
	switch( (*( p)) ) {
		case 58: goto st462;
		case 83: goto st465;
		case 115: goto st465;
	}
	goto tr257;
st462:
	if ( ++( p) == ( pe) )
		goto _test_eof462;
case 462:
	if ( (*( p)) == 47 )
		goto st463;
	goto tr257;
st463:
	if ( ++( p) == ( pe) )
		goto _test_eof463;
case 463:
	if ( (*( p)) == 47 )
		goto st464;
	goto tr257;
st464:
	if ( ++( p) == ( pe) )
		goto _test_eof464;
case 464:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st457;
st465:
	if ( ++( p) == ( pe) )
		goto _test_eof465;
case 465:
	if ( (*( p)) == 58 )
		goto st462;
	goto tr257;
tr2147:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1722;
st1722:
	if ( ++( p) == ( pe) )
		goto _test_eof1722;
case 1722:
#line 10842 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto st466;
		case 65: goto st469;
		case 67: goto st477;
		case 71: goto st486;
		case 76: goto st492;
		case 78: goto st500;
		case 80: goto st503;
		case 81: goto st509;
		case 97: goto st469;
		case 99: goto st477;
		case 103: goto st486;
		case 108: goto st492;
		case 110: goto st500;
		case 112: goto st503;
		case 113: goto st509;
	}
	goto tr2169;
st466:
	if ( ++( p) == ( pe) )
		goto _test_eof466;
case 466:
	if ( (*( p)) == 51 )
		goto st467;
	goto tr257;
st467:
	if ( ++( p) == ( pe) )
		goto _test_eof467;
case 467:
	if ( (*( p)) == 57 )
		goto st468;
	goto tr257;
st468:
	if ( ++( p) == ( pe) )
		goto _test_eof468;
case 468:
	if ( (*( p)) == 59 )
		goto tr546;
	goto tr257;
st469:
	if ( ++( p) == ( pe) )
		goto _test_eof469;
case 469:
	switch( (*( p)) ) {
		case 77: goto st470;
		case 80: goto st472;
		case 83: goto st475;
		case 109: goto st470;
		case 112: goto st472;
		case 115: goto st475;
	}
	goto tr257;
st470:
	if ( ++( p) == ( pe) )
		goto _test_eof470;
case 470:
	switch( (*( p)) ) {
		case 80: goto st471;
		case 112: goto st471;
	}
	goto tr257;
st471:
	if ( ++( p) == ( pe) )
		goto _test_eof471;
case 471:
	if ( (*( p)) == 59 )
		goto tr551;
	goto tr257;
st472:
	if ( ++( p) == ( pe) )
		goto _test_eof472;
case 472:
	switch( (*( p)) ) {
		case 79: goto st473;
		case 111: goto st473;
	}
	goto tr257;
st473:
	if ( ++( p) == ( pe) )
		goto _test_eof473;
case 473:
	switch( (*( p)) ) {
		case 83: goto st474;
		case 115: goto st474;
	}
	goto tr257;
st474:
	if ( ++( p) == ( pe) )
		goto _test_eof474;
case 474:
	if ( (*( p)) == 59 )
		goto tr554;
	goto tr257;
st475:
	if ( ++( p) == ( pe) )
		goto _test_eof475;
case 475:
	switch( (*( p)) ) {
		case 84: goto st476;
		case 116: goto st476;
	}
	goto tr257;
st476:
	if ( ++( p) == ( pe) )
		goto _test_eof476;
case 476:
	if ( (*( p)) == 59 )
		goto tr556;
	goto tr257;
st477:
	if ( ++( p) == ( pe) )
		goto _test_eof477;
case 477:
	switch( (*( p)) ) {
		case 79: goto st478;
		case 111: goto st478;
	}
	goto tr257;
st478:
	if ( ++( p) == ( pe) )
		goto _test_eof478;
case 478:
	switch( (*( p)) ) {
		case 76: goto st479;
		case 77: goto st482;
		case 108: goto st479;
		case 109: goto st482;
	}
	goto tr257;
st479:
	if ( ++( p) == ( pe) )
		goto _test_eof479;
case 479:
	switch( (*( p)) ) {
		case 79: goto st480;
		case 111: goto st480;
	}
	goto tr257;
st480:
	if ( ++( p) == ( pe) )
		goto _test_eof480;
case 480:
	switch( (*( p)) ) {
		case 78: goto st481;
		case 110: goto st481;
	}
	goto tr257;
st481:
	if ( ++( p) == ( pe) )
		goto _test_eof481;
case 481:
	if ( (*( p)) == 59 )
		goto tr562;
	goto tr257;
st482:
	if ( ++( p) == ( pe) )
		goto _test_eof482;
case 482:
	switch( (*( p)) ) {
		case 77: goto st483;
		case 109: goto st483;
	}
	goto tr257;
st483:
	if ( ++( p) == ( pe) )
		goto _test_eof483;
case 483:
	switch( (*( p)) ) {
		case 65: goto st484;
		case 97: goto st484;
	}
	goto tr257;
st484:
	if ( ++( p) == ( pe) )
		goto _test_eof484;
case 484:
	switch( (*( p)) ) {
		case 84: goto st485;
		case 116: goto st485;
	}
	goto tr257;
st485:
	if ( ++( p) == ( pe) )
		goto _test_eof485;
case 485:
	if ( (*( p)) == 59 )
		goto tr566;
	goto tr257;
st486:
	if ( ++( p) == ( pe) )
		goto _test_eof486;
case 486:
	switch( (*( p)) ) {
		case 82: goto st487;
		case 84: goto st491;
		case 114: goto st487;
		case 116: goto st491;
	}
	goto tr257;
st487:
	if ( ++( p) == ( pe) )
		goto _test_eof487;
case 487:
	switch( (*( p)) ) {
		case 65: goto st488;
		case 97: goto st488;
	}
	goto tr257;
st488:
	if ( ++( p) == ( pe) )
		goto _test_eof488;
case 488:
	switch( (*( p)) ) {
		case 86: goto st489;
		case 118: goto st489;
	}
	goto tr257;
st489:
	if ( ++( p) == ( pe) )
		goto _test_eof489;
case 489:
	switch( (*( p)) ) {
		case 69: goto st490;
		case 101: goto st490;
	}
	goto tr257;
st490:
	if ( ++( p) == ( pe) )
		goto _test_eof490;
case 490:
	if ( (*( p)) == 59 )
		goto tr572;
	goto tr257;
st491:
	if ( ++( p) == ( pe) )
		goto _test_eof491;
case 491:
	if ( (*( p)) == 59 )
		goto tr573;
	goto tr257;
st492:
	if ( ++( p) == ( pe) )
		goto _test_eof492;
case 492:
	switch( (*( p)) ) {
		case 66: goto st493;
		case 84: goto st499;
		case 98: goto st493;
		case 116: goto st499;
	}
	goto tr257;
st493:
	if ( ++( p) == ( pe) )
		goto _test_eof493;
case 493:
	switch( (*( p)) ) {
		case 82: goto st494;
		case 114: goto st494;
	}
	goto tr257;
st494:
	if ( ++( p) == ( pe) )
		goto _test_eof494;
case 494:
	switch( (*( p)) ) {
		case 65: goto st495;
		case 97: goto st495;
	}
	goto tr257;
st495:
	if ( ++( p) == ( pe) )
		goto _test_eof495;
case 495:
	switch( (*( p)) ) {
		case 67: goto st496;
		case 99: goto st496;
	}
	goto tr257;
st496:
	if ( ++( p) == ( pe) )
		goto _test_eof496;
case 496:
	switch( (*( p)) ) {
		case 69: goto st497;
		case 75: goto st498;
		case 101: goto st497;
		case 107: goto st498;
	}
	goto tr257;
st497:
	if ( ++( p) == ( pe) )
		goto _test_eof497;
case 497:
	if ( (*( p)) == 59 )
		goto tr581;
	goto tr257;
st498:
	if ( ++( p) == ( pe) )
		goto _test_eof498;
case 498:
	if ( (*( p)) == 59 )
		goto tr582;
	goto tr257;
st499:
	if ( ++( p) == ( pe) )
		goto _test_eof499;
case 499:
	if ( (*( p)) == 59 )
		goto tr583;
	goto tr257;
st500:
	if ( ++( p) == ( pe) )
		goto _test_eof500;
case 500:
	switch( (*( p)) ) {
		case 85: goto st501;
		case 117: goto st501;
	}
	goto tr257;
st501:
	if ( ++( p) == ( pe) )
		goto _test_eof501;
case 501:
	switch( (*( p)) ) {
		case 77: goto st502;
		case 109: goto st502;
	}
	goto tr257;
st502:
	if ( ++( p) == ( pe) )
		goto _test_eof502;
case 502:
	if ( (*( p)) == 59 )
		goto tr586;
	goto tr257;
st503:
	if ( ++( p) == ( pe) )
		goto _test_eof503;
case 503:
	switch( (*( p)) ) {
		case 69: goto st504;
		case 101: goto st504;
	}
	goto tr257;
st504:
	if ( ++( p) == ( pe) )
		goto _test_eof504;
case 504:
	switch( (*( p)) ) {
		case 82: goto st505;
		case 114: goto st505;
	}
	goto tr257;
st505:
	if ( ++( p) == ( pe) )
		goto _test_eof505;
case 505:
	switch( (*( p)) ) {
		case 73: goto st506;
		case 105: goto st506;
	}
	goto tr257;
st506:
	if ( ++( p) == ( pe) )
		goto _test_eof506;
case 506:
	switch( (*( p)) ) {
		case 79: goto st507;
		case 111: goto st507;
	}
	goto tr257;
st507:
	if ( ++( p) == ( pe) )
		goto _test_eof507;
case 507:
	switch( (*( p)) ) {
		case 68: goto st508;
		case 100: goto st508;
	}
	goto tr257;
st508:
	if ( ++( p) == ( pe) )
		goto _test_eof508;
case 508:
	if ( (*( p)) == 59 )
		goto tr592;
	goto tr257;
st509:
	if ( ++( p) == ( pe) )
		goto _test_eof509;
case 509:
	switch( (*( p)) ) {
		case 85: goto st510;
		case 117: goto st510;
	}
	goto tr257;
st510:
	if ( ++( p) == ( pe) )
		goto _test_eof510;
case 510:
	switch( (*( p)) ) {
		case 79: goto st511;
		case 111: goto st511;
	}
	goto tr257;
st511:
	if ( ++( p) == ( pe) )
		goto _test_eof511;
case 511:
	switch( (*( p)) ) {
		case 84: goto st512;
		case 116: goto st512;
	}
	goto tr257;
st512:
	if ( ++( p) == ( pe) )
		goto _test_eof512;
case 512:
	if ( (*( p)) == 59 )
		goto tr596;
	goto tr257;
tr2198:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1723;
tr2148:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1723;
st1723:
	if ( ++( p) == ( pe) )
		goto _test_eof1723;
case 1723:
#line 11282 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2199:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st513;
st513:
	if ( ++( p) == ( pe) )
		goto _test_eof513;
case 513:
#line 11304 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 91 )
		goto st514;
	goto tr253;
st514:
	if ( ++( p) == ( pe) )
		goto _test_eof514;
case 514:
	switch( (*( p)) ) {
		case 9: goto tr599;
		case 32: goto tr599;
		case 35: goto tr601;
		case 58: goto tr602;
		case 60: goto tr603;
		case 62: goto tr604;
		case 92: goto tr605;
		case 93: goto tr253;
		case 124: goto tr606;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr600;
	goto tr598;
tr598:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st515;
st515:
	if ( ++( p) == ( pe) )
		goto _test_eof515;
case 515:
#line 11334 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr608;
		case 32: goto tr608;
		case 35: goto tr610;
		case 93: goto tr611;
		case 124: goto tr612;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st517;
	goto st515;
tr608:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st516;
st516:
	if ( ++( p) == ( pe) )
		goto _test_eof516;
case 516:
#line 11353 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st516;
		case 32: goto st516;
		case 35: goto st518;
		case 93: goto st521;
		case 124: goto st522;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st517;
	goto st515;
tr600:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st517;
st517:
	if ( ++( p) == ( pe) )
		goto _test_eof517;
case 517:
#line 11372 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st517;
		case 93: goto tr250;
		case 124: goto tr250;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto st517;
	goto st515;
tr610:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st518;
st518:
	if ( ++( p) == ( pe) )
		goto _test_eof518;
case 518:
#line 11389 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr608;
		case 32: goto tr608;
		case 35: goto tr610;
		case 93: goto tr611;
		case 124: goto tr612;
	}
	if ( (*( p)) > 13 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr617;
	} else if ( (*( p)) >= 10 )
		goto st517;
	goto st515;
tr617:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st519;
st519:
	if ( ++( p) == ( pe) )
		goto _test_eof519;
case 519:
#line 11411 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr618;
		case 32: goto tr619;
		case 45: goto st527;
		case 93: goto tr622;
		case 95: goto st527;
		case 124: goto tr623;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st519;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st519;
	} else
		goto st519;
	goto tr250;
tr618:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st520;
st520:
	if ( ++( p) == ( pe) )
		goto _test_eof520;
case 520:
#line 11437 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st520;
		case 32: goto st520;
		case 93: goto st521;
		case 124: goto st522;
	}
	goto tr250;
tr611:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st521;
tr622:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st521;
st521:
	if ( ++( p) == ( pe) )
		goto _test_eof521;
case 521:
#line 11457 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1724;
	goto tr250;
st1724:
	if ( ++( p) == ( pe) )
		goto _test_eof1724;
case 1724:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2202;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2202;
	} else
		goto tr2202;
	goto tr2201;
tr2202:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1725;
st1725:
	if ( ++( p) == ( pe) )
		goto _test_eof1725;
case 1725:
#line 11482 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1725;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1725;
	} else
		goto st1725;
	goto tr2203;
tr612:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st522;
tr623:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st522;
tr627:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st522;
st522:
	if ( ++( p) == ( pe) )
		goto _test_eof522;
case 522:
#line 11510 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr627;
		case 32: goto tr627;
		case 93: goto tr628;
		case 124: goto tr250;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto tr626;
tr626:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st523;
st523:
	if ( ++( p) == ( pe) )
		goto _test_eof523;
case 523:
#line 11528 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr630;
		case 32: goto tr630;
		case 93: goto tr631;
		case 124: goto tr250;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st523;
tr630:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st524;
st524:
	if ( ++( p) == ( pe) )
		goto _test_eof524;
case 524:
#line 11546 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st524;
		case 32: goto st524;
		case 93: goto st525;
		case 124: goto tr250;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st523;
tr628:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st525;
tr631:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st525;
st525:
	if ( ++( p) == ( pe) )
		goto _test_eof525;
case 525:
#line 11570 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1726;
	goto tr250;
st1726:
	if ( ++( p) == ( pe) )
		goto _test_eof1726;
case 1726:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2206;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2206;
	} else
		goto tr2206;
	goto tr2205;
tr2206:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1727;
st1727:
	if ( ++( p) == ( pe) )
		goto _test_eof1727;
case 1727:
#line 11595 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1727;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1727;
	} else
		goto st1727;
	goto tr2207;
tr619:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st526;
st526:
	if ( ++( p) == ( pe) )
		goto _test_eof526;
case 526:
#line 11613 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st520;
		case 32: goto st526;
		case 45: goto st527;
		case 93: goto st521;
		case 95: goto st527;
		case 124: goto st522;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st519;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st519;
	} else
		goto st519;
	goto tr250;
st527:
	if ( ++( p) == ( pe) )
		goto _test_eof527;
case 527:
	switch( (*( p)) ) {
		case 32: goto st527;
		case 45: goto st527;
		case 95: goto st527;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st519;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st519;
	} else
		goto st519;
	goto tr250;
tr599:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st528;
st528:
	if ( ++( p) == ( pe) )
		goto _test_eof528;
case 528:
#line 11657 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr599;
		case 32: goto tr599;
		case 58: goto tr602;
		case 60: goto tr603;
		case 62: goto tr604;
		case 92: goto tr605;
		case 93: goto tr253;
		case 124: goto tr606;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr600;
	goto tr598;
tr602:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st529;
st529:
	if ( ++( p) == ( pe) )
		goto _test_eof529;
case 529:
#line 11679 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr608;
		case 32: goto tr608;
		case 35: goto tr610;
		case 93: goto tr611;
		case 124: goto tr636;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st517;
	goto st515;
tr636:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st530;
st530:
	if ( ++( p) == ( pe) )
		goto _test_eof530;
case 530:
#line 11698 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr637;
		case 32: goto tr637;
		case 35: goto tr638;
		case 93: goto tr639;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto tr626;
tr640:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st531;
tr637:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st531;
st531:
	if ( ++( p) == ( pe) )
		goto _test_eof531;
case 531:
#line 11727 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr640;
		case 32: goto tr640;
		case 35: goto tr641;
		case 93: goto tr642;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto tr626;
tr676:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st532;
tr641:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st532;
tr638:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st532;
st532:
	if ( ++( p) == ( pe) )
		goto _test_eof532;
case 532:
#line 11756 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr630;
		case 32: goto tr630;
		case 93: goto tr631;
		case 124: goto tr253;
	}
	if ( (*( p)) > 13 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr643;
	} else if ( (*( p)) >= 10 )
		goto tr253;
	goto st523;
tr643:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st533;
st533:
	if ( ++( p) == ( pe) )
		goto _test_eof533;
case 533:
#line 11777 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr644;
		case 32: goto tr645;
		case 45: goto st537;
		case 93: goto tr648;
		case 95: goto st537;
		case 124: goto tr253;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr253;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st533;
		} else if ( (*( p)) >= 65 )
			goto st533;
	} else
		goto st533;
	goto st523;
tr644:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st534;
st534:
	if ( ++( p) == ( pe) )
		goto _test_eof534;
case 534:
#line 11808 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st534;
		case 32: goto st534;
		case 93: goto st535;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto st523;
tr642:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st535;
tr639:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st535;
tr648:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st535;
tr677:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st535;
st535:
	if ( ++( p) == ( pe) )
		goto _test_eof535;
case 535:
#line 11848 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1728;
	goto tr253;
st1728:
	if ( ++( p) == ( pe) )
		goto _test_eof1728;
case 1728:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2209;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2209;
	} else
		goto tr2209;
	goto tr2201;
tr2209:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1729;
st1729:
	if ( ++( p) == ( pe) )
		goto _test_eof1729;
case 1729:
#line 11873 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1729;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1729;
	} else
		goto st1729;
	goto tr2203;
tr645:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st536;
st536:
	if ( ++( p) == ( pe) )
		goto _test_eof536;
case 536:
#line 11893 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st534;
		case 32: goto st536;
		case 45: goto st537;
		case 93: goto st535;
		case 95: goto st537;
		case 124: goto tr253;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr253;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st533;
		} else if ( (*( p)) >= 65 )
			goto st533;
	} else
		goto st533;
	goto st523;
st537:
	if ( ++( p) == ( pe) )
		goto _test_eof537;
case 537:
	switch( (*( p)) ) {
		case 9: goto tr630;
		case 32: goto tr653;
		case 45: goto st537;
		case 93: goto tr631;
		case 95: goto st537;
		case 124: goto tr253;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr253;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st533;
		} else if ( (*( p)) >= 65 )
			goto st533;
	} else
		goto st533;
	goto st523;
tr653:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st538;
st538:
	if ( ++( p) == ( pe) )
		goto _test_eof538;
case 538:
#line 11946 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st524;
		case 32: goto st538;
		case 45: goto st537;
		case 93: goto st525;
		case 95: goto st537;
		case 124: goto tr253;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto tr253;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st533;
		} else if ( (*( p)) >= 65 )
			goto st533;
	} else
		goto st533;
	goto st523;
tr603:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st539;
st539:
	if ( ++( p) == ( pe) )
		goto _test_eof539;
case 539:
#line 11975 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr608;
		case 32: goto tr608;
		case 35: goto tr610;
		case 93: goto tr611;
		case 124: goto tr655;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st517;
	goto st515;
tr655:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st540;
st540:
	if ( ++( p) == ( pe) )
		goto _test_eof540;
case 540:
#line 11994 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr627;
		case 32: goto tr627;
		case 62: goto tr656;
		case 93: goto tr628;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto tr626;
tr656:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st541;
st541:
	if ( ++( p) == ( pe) )
		goto _test_eof541;
case 541:
#line 12013 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr630;
		case 32: goto tr630;
		case 93: goto tr631;
		case 95: goto st542;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto st523;
st542:
	if ( ++( p) == ( pe) )
		goto _test_eof542;
case 542:
	switch( (*( p)) ) {
		case 9: goto tr630;
		case 32: goto tr630;
		case 60: goto st543;
		case 93: goto tr631;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto st523;
st543:
	if ( ++( p) == ( pe) )
		goto _test_eof543;
case 543:
	switch( (*( p)) ) {
		case 9: goto tr630;
		case 32: goto tr630;
		case 93: goto tr631;
		case 124: goto st544;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto st523;
st544:
	if ( ++( p) == ( pe) )
		goto _test_eof544;
case 544:
	if ( (*( p)) == 62 )
		goto st545;
	goto tr253;
st545:
	if ( ++( p) == ( pe) )
		goto _test_eof545;
case 545:
	switch( (*( p)) ) {
		case 9: goto tr661;
		case 32: goto tr661;
		case 35: goto tr662;
		case 93: goto tr611;
	}
	goto tr253;
tr661:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st546;
st546:
	if ( ++( p) == ( pe) )
		goto _test_eof546;
case 546:
#line 12077 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st546;
		case 32: goto st546;
		case 35: goto st547;
		case 93: goto st521;
	}
	goto tr253;
tr662:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st547;
st547:
	if ( ++( p) == ( pe) )
		goto _test_eof547;
case 547:
#line 12093 "ext/dtext/dtext.cpp"
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr665;
	goto tr253;
tr665:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st548;
st548:
	if ( ++( p) == ( pe) )
		goto _test_eof548;
case 548:
#line 12105 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr666;
		case 32: goto tr667;
		case 45: goto st551;
		case 93: goto tr622;
		case 95: goto st551;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st548;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st548;
	} else
		goto st548;
	goto tr253;
tr666:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st549;
st549:
	if ( ++( p) == ( pe) )
		goto _test_eof549;
case 549:
#line 12130 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st549;
		case 32: goto st549;
		case 93: goto st521;
	}
	goto tr253;
tr667:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st550;
st550:
	if ( ++( p) == ( pe) )
		goto _test_eof550;
case 550:
#line 12145 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st549;
		case 32: goto st550;
		case 45: goto st551;
		case 93: goto st521;
		case 95: goto st551;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st548;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st548;
	} else
		goto st548;
	goto tr253;
st551:
	if ( ++( p) == ( pe) )
		goto _test_eof551;
case 551:
	switch( (*( p)) ) {
		case 32: goto st551;
		case 45: goto st551;
		case 95: goto st551;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st548;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st548;
	} else
		goto st548;
	goto tr253;
tr604:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st552;
st552:
	if ( ++( p) == ( pe) )
		goto _test_eof552;
case 552:
#line 12188 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr608;
		case 32: goto tr608;
		case 35: goto tr610;
		case 58: goto st529;
		case 93: goto tr611;
		case 124: goto tr673;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st517;
	goto st515;
tr673:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st553;
st553:
	if ( ++( p) == ( pe) )
		goto _test_eof553;
case 553:
#line 12208 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr627;
		case 32: goto tr627;
		case 51: goto tr674;
		case 93: goto tr628;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto tr626;
tr674:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st554;
st554:
	if ( ++( p) == ( pe) )
		goto _test_eof554;
case 554:
#line 12227 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr675;
		case 32: goto tr675;
		case 35: goto tr676;
		case 93: goto tr677;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto st523;
tr675:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st555;
st555:
	if ( ++( p) == ( pe) )
		goto _test_eof555;
case 555:
#line 12248 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st555;
		case 32: goto st555;
		case 35: goto st532;
		case 93: goto st535;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto st523;
tr605:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st556;
st556:
	if ( ++( p) == ( pe) )
		goto _test_eof556;
case 556:
#line 12267 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr608;
		case 32: goto tr608;
		case 35: goto tr610;
		case 93: goto tr611;
		case 124: goto tr680;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto st517;
	goto st515;
tr680:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st557;
st557:
	if ( ++( p) == ( pe) )
		goto _test_eof557;
case 557:
#line 12286 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr627;
		case 32: goto tr627;
		case 93: goto tr628;
		case 124: goto st558;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto tr626;
st558:
	if ( ++( p) == ( pe) )
		goto _test_eof558;
case 558:
	if ( (*( p)) == 47 )
		goto st545;
	goto tr253;
tr606:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st559;
st559:
	if ( ++( p) == ( pe) )
		goto _test_eof559;
case 559:
#line 12311 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 95: goto st563;
		case 119: goto st564;
		case 124: goto st565;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st560;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st560;
	} else
		goto st560;
	goto tr253;
st560:
	if ( ++( p) == ( pe) )
		goto _test_eof560;
case 560:
	switch( (*( p)) ) {
		case 9: goto tr686;
		case 32: goto tr686;
		case 35: goto tr687;
		case 93: goto tr611;
		case 124: goto tr612;
	}
	goto tr253;
tr686:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st561;
st561:
	if ( ++( p) == ( pe) )
		goto _test_eof561;
case 561:
#line 12346 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st561;
		case 32: goto st561;
		case 35: goto st562;
		case 93: goto st521;
		case 124: goto st522;
	}
	goto tr253;
tr687:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st562;
st562:
	if ( ++( p) == ( pe) )
		goto _test_eof562;
case 562:
#line 12363 "ext/dtext/dtext.cpp"
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr617;
	goto tr253;
st563:
	if ( ++( p) == ( pe) )
		goto _test_eof563;
case 563:
	if ( (*( p)) == 124 )
		goto st560;
	goto tr253;
st564:
	if ( ++( p) == ( pe) )
		goto _test_eof564;
case 564:
	switch( (*( p)) ) {
		case 9: goto tr686;
		case 32: goto tr686;
		case 35: goto tr687;
		case 93: goto tr611;
		case 124: goto tr636;
	}
	goto tr253;
st565:
	if ( ++( p) == ( pe) )
		goto _test_eof565;
case 565:
	if ( (*( p)) == 95 )
		goto st566;
	goto tr253;
st566:
	if ( ++( p) == ( pe) )
		goto _test_eof566;
case 566:
	if ( (*( p)) == 124 )
		goto st563;
	goto tr253;
tr601:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st567;
st567:
	if ( ++( p) == ( pe) )
		goto _test_eof567;
case 567:
#line 12408 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr608;
		case 32: goto tr608;
		case 35: goto tr610;
		case 45: goto tr691;
		case 93: goto tr611;
		case 95: goto tr691;
		case 124: goto tr612;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto st517;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr691;
		} else if ( (*( p)) >= 65 )
			goto tr691;
	} else
		goto tr691;
	goto st515;
tr691:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st568;
st568:
	if ( ++( p) == ( pe) )
		goto _test_eof568;
case 568:
#line 12438 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr608;
		case 32: goto tr608;
		case 35: goto tr610;
		case 45: goto st568;
		case 93: goto tr693;
		case 95: goto st568;
		case 124: goto tr694;
	}
	if ( (*( p)) < 48 ) {
		if ( 10 <= (*( p)) && (*( p)) <= 13 )
			goto st517;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st568;
		} else if ( (*( p)) >= 65 )
			goto st568;
	} else
		goto st568;
	goto st515;
tr693:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st569;
st569:
	if ( ++( p) == ( pe) )
		goto _test_eof569;
case 569:
#line 12468 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1730;
	goto tr253;
st1730:
	if ( ++( p) == ( pe) )
		goto _test_eof1730;
case 1730:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2212;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2212;
	} else
		goto tr2212;
	goto tr2211;
tr2212:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1731;
st1731:
	if ( ++( p) == ( pe) )
		goto _test_eof1731;
case 1731:
#line 12493 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1731;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1731;
	} else
		goto st1731;
	goto tr2213;
tr694:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st570;
tr697:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st570;
st570:
	if ( ++( p) == ( pe) )
		goto _test_eof570;
case 570:
#line 12517 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr697;
		case 32: goto tr697;
		case 93: goto tr698;
		case 124: goto tr253;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr253;
	goto tr696;
tr696:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st571;
st571:
	if ( ++( p) == ( pe) )
		goto _test_eof571;
case 571:
#line 12535 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr700;
		case 32: goto tr700;
		case 93: goto tr701;
		case 124: goto tr250;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st571;
tr700:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st572;
st572:
	if ( ++( p) == ( pe) )
		goto _test_eof572;
case 572:
#line 12553 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st572;
		case 32: goto st572;
		case 93: goto st573;
		case 124: goto tr250;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st571;
tr698:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st573;
tr701:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st573;
st573:
	if ( ++( p) == ( pe) )
		goto _test_eof573;
case 573:
#line 12577 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 93 )
		goto st1732;
	goto tr250;
st1732:
	if ( ++( p) == ( pe) )
		goto _test_eof1732;
case 1732:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2216;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2216;
	} else
		goto tr2216;
	goto tr2215;
tr2216:
#line 81 "ext/dtext/dtext.cpp.rl"
	{ e1 = p; }
	goto st1733;
st1733:
	if ( ++( p) == ( pe) )
		goto _test_eof1733;
case 1733:
#line 12602 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1733;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1733;
	} else
		goto st1733;
	goto tr2217;
tr2200:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st574;
st574:
	if ( ++( p) == ( pe) )
		goto _test_eof574;
case 574:
#line 12620 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 123 )
		goto st575;
	goto tr253;
st575:
	if ( ++( p) == ( pe) )
		goto _test_eof575;
case 575:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto st575;
		case 32: goto st575;
		case 45: goto tr707;
		case 58: goto tr708;
		case 60: goto tr709;
		case 62: goto tr710;
		case 92: goto tr711;
		case 124: goto tr712;
		case 126: goto tr707;
	}
	if ( (*( p)) > 13 ) {
		if ( 123 <= (*( p)) && (*( p)) <= 125 )
			goto tr250;
	} else if ( (*( p)) >= 10 )
		goto tr250;
	goto tr706;
tr706:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st576;
st576:
	if ( ++( p) == ( pe) )
		goto _test_eof576;
case 576:
#line 12654 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr714;
		case 32: goto tr714;
		case 123: goto tr250;
		case 124: goto tr715;
		case 125: goto tr716;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st576;
tr714:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st577;
st577:
	if ( ++( p) == ( pe) )
		goto _test_eof577;
case 577:
#line 12674 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto st577;
		case 32: goto st577;
		case 45: goto st578;
		case 58: goto st579;
		case 60: goto st614;
		case 62: goto st615;
		case 92: goto st617;
		case 123: goto tr250;
		case 124: goto st608;
		case 125: goto st586;
		case 126: goto st578;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st576;
tr707:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st578;
st578:
	if ( ++( p) == ( pe) )
		goto _test_eof578;
case 578:
#line 12700 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr714;
		case 32: goto tr714;
		case 58: goto st579;
		case 60: goto st614;
		case 62: goto st615;
		case 92: goto st617;
		case 123: goto tr250;
		case 124: goto tr725;
		case 125: goto tr716;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st576;
tr708:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st579;
st579:
	if ( ++( p) == ( pe) )
		goto _test_eof579;
case 579:
#line 12724 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr714;
		case 32: goto tr714;
		case 123: goto st580;
		case 124: goto tr727;
		case 125: goto tr728;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st576;
st580:
	if ( ++( p) == ( pe) )
		goto _test_eof580;
case 580:
	switch( (*( p)) ) {
		case 9: goto tr714;
		case 32: goto tr714;
		case 124: goto tr715;
		case 125: goto tr716;
	}
	goto tr250;
tr715:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st581;
tr730:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st581;
tr742:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st581;
st581:
	if ( ++( p) == ( pe) )
		goto _test_eof581;
case 581:
#line 12767 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr730;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr730;
		case 125: goto tr732;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr731;
	goto tr729;
tr729:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st582;
st582:
	if ( ++( p) == ( pe) )
		goto _test_eof582;
case 582:
#line 12787 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr734:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st583;
st583:
	if ( ++( p) == ( pe) )
		goto _test_eof583;
case 583:
#line 12807 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto st583;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto st583;
		case 125: goto st585;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr731:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st584;
st584:
	if ( ++( p) == ( pe) )
		goto _test_eof584;
case 584:
#line 12827 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto st584;
		case 125: goto tr250;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr736:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st585;
tr732:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st585;
st585:
	if ( ++( p) == ( pe) )
		goto _test_eof585;
case 585:
#line 12852 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1734;
	goto tr250;
st1734:
	if ( ++( p) == ( pe) )
		goto _test_eof1734;
case 1734:
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
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1735;
st1735:
	if ( ++( p) == ( pe) )
		goto _test_eof1735;
case 1735:
#line 12877 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1735;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1735;
	} else
		goto st1735;
	goto tr2221;
tr716:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st586;
st586:
	if ( ++( p) == ( pe) )
		goto _test_eof586;
case 586:
#line 12895 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1736;
	goto tr250;
tr2229:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1736;
st1736:
	if ( ++( p) == ( pe) )
		goto _test_eof1736;
case 1736:
#line 12909 "ext/dtext/dtext.cpp"
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
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1737;
st1737:
	if ( ++( p) == ( pe) )
		goto _test_eof1737;
case 1737:
#line 12927 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1737;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1737;
	} else
		goto st1737;
	goto tr2225;
tr727:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st587;
st587:
	if ( ++( p) == ( pe) )
		goto _test_eof587;
case 587:
#line 12945 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr741;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr741;
		case 124: goto tr742;
		case 125: goto tr743;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr731;
	goto tr729;
tr745:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st588;
tr741:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st588;
st588:
	if ( ++( p) == ( pe) )
		goto _test_eof588;
case 588:
#line 12976 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr745;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr745;
		case 45: goto tr746;
		case 58: goto tr747;
		case 60: goto tr748;
		case 62: goto tr749;
		case 92: goto tr750;
		case 123: goto tr729;
		case 124: goto tr751;
		case 125: goto tr752;
		case 126: goto tr746;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr731;
	goto tr744;
tr744:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st589;
st589:
	if ( ++( p) == ( pe) )
		goto _test_eof589;
case 589:
#line 13004 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 123: goto st582;
		case 124: goto tr715;
		case 125: goto tr755;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st589;
tr754:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st590;
st590:
	if ( ++( p) == ( pe) )
		goto _test_eof590;
case 590:
#line 13028 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto st590;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto st590;
		case 45: goto st591;
		case 58: goto st592;
		case 60: goto st596;
		case 62: goto st602;
		case 92: goto st605;
		case 123: goto st582;
		case 124: goto st608;
		case 125: goto st594;
		case 126: goto st591;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st589;
tr746:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st591;
st591:
	if ( ++( p) == ( pe) )
		goto _test_eof591;
case 591:
#line 13056 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 58: goto st592;
		case 60: goto st596;
		case 62: goto st602;
		case 92: goto st605;
		case 123: goto st582;
		case 124: goto tr725;
		case 125: goto tr755;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st589;
tr747:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st592;
st592:
	if ( ++( p) == ( pe) )
		goto _test_eof592;
case 592:
#line 13082 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 123: goto st593;
		case 124: goto tr727;
		case 125: goto tr764;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st589;
tr774:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st593;
st593:
	if ( ++( p) == ( pe) )
		goto _test_eof593;
case 593:
#line 13104 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 124: goto tr715;
		case 125: goto tr755;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr752:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st594;
tr743:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st594;
tr755:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st594;
st594:
	if ( ++( p) == ( pe) )
		goto _test_eof594;
case 594:
#line 13141 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1738;
	goto tr250;
st1738:
	if ( ++( p) == ( pe) )
		goto _test_eof1738;
case 1738:
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2227;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2227;
	} else
		goto tr2227;
	goto tr2223;
tr2227:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1739;
st1739:
	if ( ++( p) == ( pe) )
		goto _test_eof1739;
case 1739:
#line 13166 "ext/dtext/dtext.cpp"
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1739;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1739;
	} else
		goto st1739;
	goto tr2225;
tr764:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st595;
st595:
	if ( ++( p) == ( pe) )
		goto _test_eof595;
case 595:
#line 13186 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr714;
		case 32: goto tr714;
		case 124: goto tr715;
		case 125: goto tr766;
	}
	goto tr250;
tr766:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1740;
st1740:
	if ( ++( p) == ( pe) )
		goto _test_eof1740;
case 1740:
#line 13202 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto tr2229;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2227;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2227;
	} else
		goto tr2227;
	goto tr2223;
tr748:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st596;
st596:
	if ( ++( p) == ( pe) )
		goto _test_eof596;
case 596:
#line 13222 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 123: goto st582;
		case 124: goto tr767;
		case 125: goto tr755;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st589;
tr767:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st597;
st597:
	if ( ++( p) == ( pe) )
		goto _test_eof597;
case 597:
#line 13244 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr730;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr730;
		case 62: goto tr768;
		case 125: goto tr732;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr731;
	goto tr729;
tr768:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st598;
st598:
	if ( ++( p) == ( pe) )
		goto _test_eof598;
case 598:
#line 13265 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 95: goto st599;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
st599:
	if ( ++( p) == ( pe) )
		goto _test_eof599;
case 599:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 60: goto st600;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
st600:
	if ( ++( p) == ( pe) )
		goto _test_eof600;
case 600:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 124: goto st601;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
st601:
	if ( ++( p) == ( pe) )
		goto _test_eof601;
case 601:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 62: goto st593;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr749:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st602;
st602:
	if ( ++( p) == ( pe) )
		goto _test_eof602;
case 602:
#line 13334 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 58: goto st603;
		case 123: goto st582;
		case 124: goto tr773;
		case 125: goto tr755;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st589;
st603:
	if ( ++( p) == ( pe) )
		goto _test_eof603;
case 603:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 123: goto st582;
		case 124: goto tr727;
		case 125: goto tr755;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st589;
tr773:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st604;
st604:
	if ( ++( p) == ( pe) )
		goto _test_eof604;
case 604:
#line 13374 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr730;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr730;
		case 51: goto tr774;
		case 125: goto tr732;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr731;
	goto tr729;
tr750:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st605;
st605:
	if ( ++( p) == ( pe) )
		goto _test_eof605;
case 605:
#line 13395 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 123: goto st582;
		case 124: goto tr775;
		case 125: goto tr755;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st589;
tr775:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st606;
st606:
	if ( ++( p) == ( pe) )
		goto _test_eof606;
case 606:
#line 13417 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr730;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr730;
		case 124: goto tr776;
		case 125: goto tr732;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr731;
	goto tr729;
tr776:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st607;
st607:
	if ( ++( p) == ( pe) )
		goto _test_eof607;
case 607:
#line 13438 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 47: goto st593;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr725:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st608;
tr751:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st608;
st608:
	if ( ++( p) == ( pe) )
		goto _test_eof608;
case 608:
#line 13463 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr730;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr730;
		case 95: goto tr777;
		case 119: goto tr778;
		case 124: goto tr779;
		case 125: goto tr732;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto tr731;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr774;
		} else if ( (*( p)) >= 65 )
			goto tr774;
	} else
		goto tr774;
	goto tr729;
tr777:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st609;
st609:
	if ( ++( p) == ( pe) )
		goto _test_eof609;
case 609:
#line 13495 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 124: goto st593;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr778:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st610;
st610:
	if ( ++( p) == ( pe) )
		goto _test_eof610;
case 610:
#line 13516 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr754;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr754;
		case 124: goto tr727;
		case 125: goto tr755;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr779:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st611;
st611:
	if ( ++( p) == ( pe) )
		goto _test_eof611;
case 611:
#line 13537 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 95: goto st612;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
st612:
	if ( ++( p) == ( pe) )
		goto _test_eof612;
case 612:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr734;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr734;
		case 124: goto st609;
		case 125: goto tr736;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st584;
	goto st582;
tr728:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st613;
st613:
	if ( ++( p) == ( pe) )
		goto _test_eof613;
case 613:
#line 13574 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr714;
		case 32: goto tr714;
		case 124: goto tr715;
		case 125: goto tr782;
	}
	goto tr250;
tr782:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1741;
st1741:
	if ( ++( p) == ( pe) )
		goto _test_eof1741;
case 1741:
#line 13590 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 125 )
		goto st1736;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2224;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2224;
	} else
		goto tr2224;
	goto tr2223;
tr709:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st614;
st614:
	if ( ++( p) == ( pe) )
		goto _test_eof614;
case 614:
#line 13610 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr714;
		case 32: goto tr714;
		case 123: goto tr250;
		case 124: goto tr767;
		case 125: goto tr716;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st576;
tr710:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st615;
st615:
	if ( ++( p) == ( pe) )
		goto _test_eof615;
case 615:
#line 13630 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr714;
		case 32: goto tr714;
		case 58: goto st616;
		case 123: goto tr250;
		case 124: goto tr773;
		case 125: goto tr716;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st576;
st616:
	if ( ++( p) == ( pe) )
		goto _test_eof616;
case 616:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr714;
		case 32: goto tr714;
		case 123: goto tr250;
		case 124: goto tr727;
		case 125: goto tr716;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st576;
tr711:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st617;
st617:
	if ( ++( p) == ( pe) )
		goto _test_eof617;
case 617:
#line 13666 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr714;
		case 32: goto tr714;
		case 123: goto tr250;
		case 124: goto tr775;
		case 125: goto tr716;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st576;
tr712:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st618;
st618:
	if ( ++( p) == ( pe) )
		goto _test_eof618;
case 618:
#line 13686 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 95: goto st619;
		case 119: goto st620;
		case 124: goto st621;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st580;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st580;
	} else
		goto st580;
	goto tr250;
st619:
	if ( ++( p) == ( pe) )
		goto _test_eof619;
case 619:
	if ( (*( p)) == 124 )
		goto st580;
	goto tr250;
st620:
	if ( ++( p) == ( pe) )
		goto _test_eof620;
case 620:
	switch( (*( p)) ) {
		case 9: goto tr714;
		case 32: goto tr714;
		case 124: goto tr727;
		case 125: goto tr716;
	}
	goto tr250;
st621:
	if ( ++( p) == ( pe) )
		goto _test_eof621;
case 621:
	if ( (*( p)) == 95 )
		goto st622;
	goto tr250;
st622:
	if ( ++( p) == ( pe) )
		goto _test_eof622;
case 622:
	if ( (*( p)) == 124 )
		goto st619;
	goto tr250;
tr2149:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1742;
st1742:
	if ( ++( p) == ( pe) )
		goto _test_eof1742;
case 1742:
#line 13745 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2230;
		case 82: goto tr2231;
		case 86: goto tr2232;
		case 91: goto tr2199;
		case 108: goto tr2230;
		case 114: goto tr2231;
		case 118: goto tr2232;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2230:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1743;
st1743:
	if ( ++( p) == ( pe) )
		goto _test_eof1743;
case 1743:
#line 13775 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2233;
		case 91: goto tr2199;
		case 105: goto tr2233;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2233:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1744;
st1744:
	if ( ++( p) == ( pe) )
		goto _test_eof1744;
case 1744:
#line 13801 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2234;
		case 91: goto tr2199;
		case 97: goto tr2234;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2234:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1745;
st1745:
	if ( ++( p) == ( pe) )
		goto _test_eof1745;
case 1745:
#line 13827 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2235;
		case 91: goto tr2199;
		case 115: goto tr2235;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2235:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1746;
st1746:
	if ( ++( p) == ( pe) )
		goto _test_eof1746;
case 1746:
#line 13853 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st623;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st623:
	if ( ++( p) == ( pe) )
		goto _test_eof623;
case 623:
	if ( (*( p)) == 35 )
		goto st624;
	goto tr253;
st624:
	if ( ++( p) == ( pe) )
		goto _test_eof624;
case 624:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr789;
	goto tr253;
tr789:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1747;
st1747:
	if ( ++( p) == ( pe) )
		goto _test_eof1747;
case 1747:
#line 13890 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1747;
	goto tr2237;
tr2231:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1748;
st1748:
	if ( ++( p) == ( pe) )
		goto _test_eof1748;
case 1748:
#line 13904 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2239;
		case 91: goto tr2199;
		case 116: goto tr2239;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2239:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1749;
st1749:
	if ( ++( p) == ( pe) )
		goto _test_eof1749;
case 1749:
#line 13930 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2240;
		case 91: goto tr2199;
		case 105: goto tr2240;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2240:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1750;
st1750:
	if ( ++( p) == ( pe) )
		goto _test_eof1750;
case 1750:
#line 13956 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2241;
		case 91: goto tr2199;
		case 115: goto tr2241;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 13982 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2242;
		case 91: goto tr2199;
		case 116: goto tr2242;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 14008 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st625;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st625:
	if ( ++( p) == ( pe) )
		goto _test_eof625;
case 625:
	switch( (*( p)) ) {
		case 35: goto st626;
		case 67: goto st627;
		case 99: goto st627;
	}
	goto tr253;
st626:
	if ( ++( p) == ( pe) )
		goto _test_eof626;
case 626:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr792;
	goto tr253;
tr792:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1753;
st1753:
	if ( ++( p) == ( pe) )
		goto _test_eof1753;
case 1753:
#line 14048 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1753;
	goto tr2244;
st627:
	if ( ++( p) == ( pe) )
		goto _test_eof627;
case 627:
	switch( (*( p)) ) {
		case 72: goto st628;
		case 104: goto st628;
	}
	goto tr253;
st628:
	if ( ++( p) == ( pe) )
		goto _test_eof628;
case 628:
	switch( (*( p)) ) {
		case 65: goto st629;
		case 97: goto st629;
	}
	goto tr253;
st629:
	if ( ++( p) == ( pe) )
		goto _test_eof629;
case 629:
	switch( (*( p)) ) {
		case 78: goto st630;
		case 110: goto st630;
	}
	goto tr253;
st630:
	if ( ++( p) == ( pe) )
		goto _test_eof630;
case 630:
	switch( (*( p)) ) {
		case 71: goto st631;
		case 103: goto st631;
	}
	goto tr253;
st631:
	if ( ++( p) == ( pe) )
		goto _test_eof631;
case 631:
	switch( (*( p)) ) {
		case 69: goto st632;
		case 101: goto st632;
	}
	goto tr253;
st632:
	if ( ++( p) == ( pe) )
		goto _test_eof632;
case 632:
	switch( (*( p)) ) {
		case 83: goto st633;
		case 115: goto st633;
	}
	goto tr253;
st633:
	if ( ++( p) == ( pe) )
		goto _test_eof633;
case 633:
	if ( (*( p)) == 32 )
		goto st634;
	goto tr253;
st634:
	if ( ++( p) == ( pe) )
		goto _test_eof634;
case 634:
	if ( (*( p)) == 35 )
		goto st635;
	goto tr253;
st635:
	if ( ++( p) == ( pe) )
		goto _test_eof635;
case 635:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr801;
	goto tr253;
tr801:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1754;
st1754:
	if ( ++( p) == ( pe) )
		goto _test_eof1754;
case 1754:
#line 14135 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1754;
	goto tr2246;
tr2232:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1755;
st1755:
	if ( ++( p) == ( pe) )
		goto _test_eof1755;
case 1755:
#line 14149 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2248;
		case 91: goto tr2199;
		case 111: goto tr2248;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2248:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1756;
st1756:
	if ( ++( p) == ( pe) )
		goto _test_eof1756;
case 1756:
#line 14175 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2249;
		case 91: goto tr2199;
		case 105: goto tr2249;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2249:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1757;
st1757:
	if ( ++( p) == ( pe) )
		goto _test_eof1757;
case 1757:
#line 14201 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2250;
		case 91: goto tr2199;
		case 100: goto tr2250;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2250:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1758;
st1758:
	if ( ++( p) == ( pe) )
		goto _test_eof1758;
case 1758:
#line 14227 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st636;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st636:
	if ( ++( p) == ( pe) )
		goto _test_eof636;
case 636:
	switch( (*( p)) ) {
		case 80: goto st637;
		case 112: goto st637;
	}
	goto tr253;
st637:
	if ( ++( p) == ( pe) )
		goto _test_eof637;
case 637:
	switch( (*( p)) ) {
		case 79: goto st638;
		case 111: goto st638;
	}
	goto tr253;
st638:
	if ( ++( p) == ( pe) )
		goto _test_eof638;
case 638:
	switch( (*( p)) ) {
		case 83: goto st639;
		case 115: goto st639;
	}
	goto tr253;
st639:
	if ( ++( p) == ( pe) )
		goto _test_eof639;
case 639:
	switch( (*( p)) ) {
		case 84: goto st640;
		case 116: goto st640;
	}
	goto tr253;
st640:
	if ( ++( p) == ( pe) )
		goto _test_eof640;
case 640:
	switch( (*( p)) ) {
		case 73: goto st641;
		case 105: goto st641;
	}
	goto tr253;
st641:
	if ( ++( p) == ( pe) )
		goto _test_eof641;
case 641:
	switch( (*( p)) ) {
		case 78: goto st642;
		case 110: goto st642;
	}
	goto tr253;
st642:
	if ( ++( p) == ( pe) )
		goto _test_eof642;
case 642:
	switch( (*( p)) ) {
		case 71: goto st643;
		case 103: goto st643;
	}
	goto tr253;
st643:
	if ( ++( p) == ( pe) )
		goto _test_eof643;
case 643:
	if ( (*( p)) == 32 )
		goto st644;
	goto tr253;
st644:
	if ( ++( p) == ( pe) )
		goto _test_eof644;
case 644:
	if ( (*( p)) == 35 )
		goto st645;
	goto tr253;
st645:
	if ( ++( p) == ( pe) )
		goto _test_eof645;
case 645:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr811;
	goto tr253;
tr811:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1759;
st1759:
	if ( ++( p) == ( pe) )
		goto _test_eof1759;
case 1759:
#line 14334 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1759;
	goto tr2252;
tr2150:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1760;
st1760:
	if ( ++( p) == ( pe) )
		goto _test_eof1760;
case 1760:
#line 14350 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2254;
		case 85: goto tr2255;
		case 91: goto tr2199;
		case 97: goto tr2254;
		case 117: goto tr2255;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2254:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1761;
st1761:
	if ( ++( p) == ( pe) )
		goto _test_eof1761;
case 1761:
#line 14378 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2256;
		case 91: goto tr2199;
		case 110: goto tr2256;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2256:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1762;
st1762:
	if ( ++( p) == ( pe) )
		goto _test_eof1762;
case 1762:
#line 14404 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st646;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st646:
	if ( ++( p) == ( pe) )
		goto _test_eof646;
case 646:
	if ( (*( p)) == 35 )
		goto st647;
	goto tr253;
st647:
	if ( ++( p) == ( pe) )
		goto _test_eof647;
case 647:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr813;
	goto tr253;
tr813:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1763;
st1763:
	if ( ++( p) == ( pe) )
		goto _test_eof1763;
case 1763:
#line 14441 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1763;
	goto tr2258;
tr2255:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1764;
st1764:
	if ( ++( p) == ( pe) )
		goto _test_eof1764;
case 1764:
#line 14455 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2260;
		case 91: goto tr2199;
		case 114: goto tr2260;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2260:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1765;
st1765:
	if ( ++( p) == ( pe) )
		goto _test_eof1765;
case 1765:
#line 14481 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st648;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st648:
	if ( ++( p) == ( pe) )
		goto _test_eof648;
case 648:
	if ( (*( p)) == 35 )
		goto st649;
	goto tr253;
st649:
	if ( ++( p) == ( pe) )
		goto _test_eof649;
case 649:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr815;
	goto tr253;
tr815:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1766;
st1766:
	if ( ++( p) == ( pe) )
		goto _test_eof1766;
case 1766:
#line 14518 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1766;
	goto tr2262;
tr2151:
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
#line 14534 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2264;
		case 91: goto tr2199;
		case 111: goto tr2264;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2264:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1768;
st1768:
	if ( ++( p) == ( pe) )
		goto _test_eof1768;
case 1768:
#line 14560 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2265;
		case 91: goto tr2199;
		case 109: goto tr2265;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2265:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1769;
st1769:
	if ( ++( p) == ( pe) )
		goto _test_eof1769;
case 1769:
#line 14586 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2266;
		case 91: goto tr2199;
		case 109: goto tr2266;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2266:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1770;
st1770:
	if ( ++( p) == ( pe) )
		goto _test_eof1770;
case 1770:
#line 14612 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2267;
		case 73: goto tr2268;
		case 91: goto tr2199;
		case 101: goto tr2267;
		case 105: goto tr2268;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2267:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1771;
st1771:
	if ( ++( p) == ( pe) )
		goto _test_eof1771;
case 1771:
#line 14640 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2269;
		case 91: goto tr2199;
		case 110: goto tr2269;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2269:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1772;
st1772:
	if ( ++( p) == ( pe) )
		goto _test_eof1772;
case 1772:
#line 14666 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2270;
		case 91: goto tr2199;
		case 116: goto tr2270;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2270:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1773;
st1773:
	if ( ++( p) == ( pe) )
		goto _test_eof1773;
case 1773:
#line 14692 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st650;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st650:
	if ( ++( p) == ( pe) )
		goto _test_eof650;
case 650:
	if ( (*( p)) == 35 )
		goto st651;
	goto tr253;
st651:
	if ( ++( p) == ( pe) )
		goto _test_eof651;
case 651:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr817;
	goto tr253;
tr817:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1774;
st1774:
	if ( ++( p) == ( pe) )
		goto _test_eof1774;
case 1774:
#line 14729 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1774;
	goto tr2272;
tr2268:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1775;
st1775:
	if ( ++( p) == ( pe) )
		goto _test_eof1775;
case 1775:
#line 14743 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2274;
		case 91: goto tr2199;
		case 116: goto tr2274;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2274:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1776;
st1776:
	if ( ++( p) == ( pe) )
		goto _test_eof1776;
case 1776:
#line 14769 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st652;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st652:
	if ( ++( p) == ( pe) )
		goto _test_eof652;
case 652:
	if ( (*( p)) == 35 )
		goto st653;
	goto tr253;
st653:
	if ( ++( p) == ( pe) )
		goto _test_eof653;
case 653:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr819;
	goto tr253;
tr819:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1777;
st1777:
	if ( ++( p) == ( pe) )
		goto _test_eof1777;
case 1777:
#line 14806 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1777;
	goto tr2276;
tr2152:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1778;
st1778:
	if ( ++( p) == ( pe) )
		goto _test_eof1778;
case 1778:
#line 14822 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2278;
		case 78: goto tr2279;
		case 91: goto tr2199;
		case 109: goto tr2278;
		case 110: goto tr2279;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2278:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1779;
st1779:
	if ( ++( p) == ( pe) )
		goto _test_eof1779;
case 1779:
#line 14850 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2280;
		case 91: goto tr2199;
		case 97: goto tr2280;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2280:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1780;
st1780:
	if ( ++( p) == ( pe) )
		goto _test_eof1780;
case 1780:
#line 14876 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2281;
		case 91: goto tr2199;
		case 105: goto tr2281;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2281:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1781;
st1781:
	if ( ++( p) == ( pe) )
		goto _test_eof1781;
case 1781:
#line 14902 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2282;
		case 91: goto tr2199;
		case 108: goto tr2282;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2282:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1782;
st1782:
	if ( ++( p) == ( pe) )
		goto _test_eof1782;
case 1782:
#line 14928 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st654;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st654:
	if ( ++( p) == ( pe) )
		goto _test_eof654;
case 654:
	if ( (*( p)) == 35 )
		goto st655;
	goto tr253;
st655:
	if ( ++( p) == ( pe) )
		goto _test_eof655;
case 655:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr821;
	goto tr253;
tr2286:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1783;
tr821:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1783;
st1783:
	if ( ++( p) == ( pe) )
		goto _test_eof1783;
case 1783:
#line 14971 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto tr2285;
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr2286;
	goto tr2284;
tr2285:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st656;
st656:
	if ( ++( p) == ( pe) )
		goto _test_eof656;
case 656:
#line 14985 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 45: goto tr823;
		case 61: goto tr823;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr823;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr823;
	} else
		goto tr823;
	goto tr822;
tr823:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1784;
st1784:
	if ( ++( p) == ( pe) )
		goto _test_eof1784;
case 1784:
#line 15007 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 45: goto st1784;
		case 61: goto st1784;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1784;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1784;
	} else
		goto st1784;
	goto tr2287;
tr2279:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1785;
st1785:
	if ( ++( p) == ( pe) )
		goto _test_eof1785;
case 1785:
#line 15031 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2289;
		case 91: goto tr2199;
		case 112: goto tr2289;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2289:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1786;
st1786:
	if ( ++( p) == ( pe) )
		goto _test_eof1786;
case 1786:
#line 15057 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st657;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st657:
	if ( ++( p) == ( pe) )
		goto _test_eof657;
case 657:
	if ( (*( p)) == 35 )
		goto st658;
	goto tr253;
st658:
	if ( ++( p) == ( pe) )
		goto _test_eof658;
case 658:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr825;
	goto tr253;
tr825:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1787;
st1787:
	if ( ++( p) == ( pe) )
		goto _test_eof1787;
case 1787:
#line 15094 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1787;
	goto tr2291;
tr2153:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1788;
st1788:
	if ( ++( p) == ( pe) )
		goto _test_eof1788;
case 1788:
#line 15110 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2293;
		case 79: goto tr2294;
		case 91: goto tr2199;
		case 108: goto tr2293;
		case 111: goto tr2294;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2293:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1789;
st1789:
	if ( ++( p) == ( pe) )
		goto _test_eof1789;
case 1789:
#line 15138 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2295;
		case 91: goto tr2199;
		case 97: goto tr2295;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2295:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1790;
st1790:
	if ( ++( p) == ( pe) )
		goto _test_eof1790;
case 1790:
#line 15164 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2296;
		case 91: goto tr2199;
		case 103: goto tr2296;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2296:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1791;
st1791:
	if ( ++( p) == ( pe) )
		goto _test_eof1791;
case 1791:
#line 15190 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st659;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st659:
	if ( ++( p) == ( pe) )
		goto _test_eof659;
case 659:
	if ( (*( p)) == 35 )
		goto st660;
	goto tr253;
st660:
	if ( ++( p) == ( pe) )
		goto _test_eof660;
case 660:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr827;
	goto tr253;
tr827:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1792;
st1792:
	if ( ++( p) == ( pe) )
		goto _test_eof1792;
case 1792:
#line 15227 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1792;
	goto tr2298;
tr2294:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1793;
st1793:
	if ( ++( p) == ( pe) )
		goto _test_eof1793;
case 1793:
#line 15241 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2300;
		case 91: goto tr2199;
		case 114: goto tr2300;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2300:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1794;
st1794:
	if ( ++( p) == ( pe) )
		goto _test_eof1794;
case 1794:
#line 15267 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2301;
		case 91: goto tr2199;
		case 117: goto tr2301;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2301:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1795;
st1795:
	if ( ++( p) == ( pe) )
		goto _test_eof1795;
case 1795:
#line 15293 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2302;
		case 91: goto tr2199;
		case 109: goto tr2302;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2302:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1796;
st1796:
	if ( ++( p) == ( pe) )
		goto _test_eof1796;
case 1796:
#line 15319 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st661;
		case 80: goto tr2304;
		case 91: goto tr2199;
		case 112: goto tr2304;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st661:
	if ( ++( p) == ( pe) )
		goto _test_eof661;
case 661:
	switch( (*( p)) ) {
		case 32: goto st662;
		case 35: goto st663;
		case 80: goto st664;
		case 84: goto st668;
		case 112: goto st664;
		case 116: goto st668;
	}
	goto tr253;
st662:
	if ( ++( p) == ( pe) )
		goto _test_eof662;
case 662:
	if ( (*( p)) == 35 )
		goto st663;
	goto tr253;
st663:
	if ( ++( p) == ( pe) )
		goto _test_eof663;
case 663:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr832;
	goto tr253;
tr832:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1797;
st1797:
	if ( ++( p) == ( pe) )
		goto _test_eof1797;
case 1797:
#line 15371 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1797;
	goto tr2305;
st664:
	if ( ++( p) == ( pe) )
		goto _test_eof664;
case 664:
	switch( (*( p)) ) {
		case 79: goto st665;
		case 111: goto st665;
	}
	goto tr253;
st665:
	if ( ++( p) == ( pe) )
		goto _test_eof665;
case 665:
	switch( (*( p)) ) {
		case 83: goto st666;
		case 115: goto st666;
	}
	goto tr253;
st666:
	if ( ++( p) == ( pe) )
		goto _test_eof666;
case 666:
	switch( (*( p)) ) {
		case 84: goto st667;
		case 116: goto st667;
	}
	goto tr253;
st667:
	if ( ++( p) == ( pe) )
		goto _test_eof667;
case 667:
	if ( (*( p)) == 32 )
		goto st662;
	goto tr253;
st668:
	if ( ++( p) == ( pe) )
		goto _test_eof668;
case 668:
	switch( (*( p)) ) {
		case 79: goto st669;
		case 111: goto st669;
	}
	goto tr253;
st669:
	if ( ++( p) == ( pe) )
		goto _test_eof669;
case 669:
	switch( (*( p)) ) {
		case 80: goto st670;
		case 112: goto st670;
	}
	goto tr253;
st670:
	if ( ++( p) == ( pe) )
		goto _test_eof670;
case 670:
	switch( (*( p)) ) {
		case 73: goto st671;
		case 105: goto st671;
	}
	goto tr253;
st671:
	if ( ++( p) == ( pe) )
		goto _test_eof671;
case 671:
	switch( (*( p)) ) {
		case 67: goto st672;
		case 99: goto st672;
	}
	goto tr253;
st672:
	if ( ++( p) == ( pe) )
		goto _test_eof672;
case 672:
	if ( (*( p)) == 32 )
		goto st673;
	goto tr253;
st673:
	if ( ++( p) == ( pe) )
		goto _test_eof673;
case 673:
	if ( (*( p)) == 35 )
		goto st674;
	goto tr253;
st674:
	if ( ++( p) == ( pe) )
		goto _test_eof674;
case 674:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr842;
	goto tr253;
tr2309:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1798;
tr842:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1798;
st1798:
	if ( ++( p) == ( pe) )
		goto _test_eof1798;
case 1798:
#line 15480 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto tr2308;
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr2309;
	goto tr2307;
tr2308:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st675;
st675:
	if ( ++( p) == ( pe) )
		goto _test_eof675;
case 675:
#line 15494 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto st676;
		case 112: goto st676;
	}
	goto tr843;
st676:
	if ( ++( p) == ( pe) )
		goto _test_eof676;
case 676:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr845;
	goto tr843;
tr845:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1799;
st1799:
	if ( ++( p) == ( pe) )
		goto _test_eof1799;
case 1799:
#line 15515 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1799;
	goto tr2310;
tr2304:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1800;
st1800:
	if ( ++( p) == ( pe) )
		goto _test_eof1800;
case 1800:
#line 15529 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2312;
		case 91: goto tr2199;
		case 111: goto tr2312;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2312:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1801;
st1801:
	if ( ++( p) == ( pe) )
		goto _test_eof1801;
case 1801:
#line 15555 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2313;
		case 91: goto tr2199;
		case 115: goto tr2313;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2313:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1802;
st1802:
	if ( ++( p) == ( pe) )
		goto _test_eof1802;
case 1802:
#line 15581 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2314;
		case 91: goto tr2199;
		case 116: goto tr2314;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2314:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1803;
st1803:
	if ( ++( p) == ( pe) )
		goto _test_eof1803;
case 1803:
#line 15607 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st662;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2154:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1804;
st1804:
	if ( ++( p) == ( pe) )
		goto _test_eof1804;
case 1804:
#line 15634 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2315;
		case 91: goto tr2199;
		case 116: goto tr2315;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2315:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1805;
st1805:
	if ( ++( p) == ( pe) )
		goto _test_eof1805;
case 1805:
#line 15660 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2316;
		case 91: goto tr2199;
		case 116: goto tr2316;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2316:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1806;
st1806:
	if ( ++( p) == ( pe) )
		goto _test_eof1806;
case 1806:
#line 15686 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2317;
		case 91: goto tr2199;
		case 112: goto tr2317;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2317:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1807;
st1807:
	if ( ++( p) == ( pe) )
		goto _test_eof1807;
case 1807:
#line 15712 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 58: goto st677;
		case 83: goto tr2319;
		case 91: goto tr2199;
		case 115: goto tr2319;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st677:
	if ( ++( p) == ( pe) )
		goto _test_eof677;
case 677:
	if ( (*( p)) == 47 )
		goto st678;
	goto tr253;
st678:
	if ( ++( p) == ( pe) )
		goto _test_eof678;
case 678:
	if ( (*( p)) == 47 )
		goto st679;
	goto tr253;
st679:
	if ( ++( p) == ( pe) )
		goto _test_eof679;
case 679:
	switch( (*( p)) ) {
		case 45: goto st681;
		case 95: goto st681;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st682;
		} else if ( (*( p)) >= -62 )
			goto st680;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st681;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st681;
		} else
			goto st681;
	} else
		goto st683;
	goto tr253;
st680:
	if ( ++( p) == ( pe) )
		goto _test_eof680;
case 680:
	if ( (*( p)) <= -65 )
		goto st681;
	goto tr253;
st681:
	if ( ++( p) == ( pe) )
		goto _test_eof681;
case 681:
	switch( (*( p)) ) {
		case 45: goto st681;
		case 46: goto st684;
		case 95: goto st681;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -17 )
				goto st682;
		} else if ( (*( p)) >= -62 )
			goto st680;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto st681;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st681;
		} else
			goto st681;
	} else
		goto st683;
	goto tr253;
st682:
	if ( ++( p) == ( pe) )
		goto _test_eof682;
case 682:
	if ( (*( p)) <= -65 )
		goto st680;
	goto tr253;
st683:
	if ( ++( p) == ( pe) )
		goto _test_eof683;
case 683:
	if ( (*( p)) <= -65 )
		goto st682;
	goto tr253;
st684:
	if ( ++( p) == ( pe) )
		goto _test_eof684;
case 684:
	switch( (*( p)) ) {
		case -30: goto st687;
		case -29: goto st690;
		case -17: goto st692;
		case 45: goto tr859;
		case 95: goto tr859;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st686;
		} else if ( (*( p)) >= -62 )
			goto st685;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 48 <= (*( p)) && (*( p)) <= 57 )
				goto tr859;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr859;
		} else
			goto tr859;
	} else
		goto st695;
	goto tr250;
st685:
	if ( ++( p) == ( pe) )
		goto _test_eof685;
case 685:
	if ( (*( p)) <= -65 )
		goto tr859;
	goto tr250;
tr859:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 410 "ext/dtext/dtext.cpp.rl"
	{( act) = 56;}
	goto st1808;
st1808:
	if ( ++( p) == ( pe) )
		goto _test_eof1808;
case 1808:
#line 15863 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st687;
		case -29: goto st690;
		case -17: goto st692;
		case 35: goto tr862;
		case 46: goto st684;
		case 47: goto tr863;
		case 58: goto st729;
		case 63: goto st718;
		case 95: goto tr859;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st686;
		} else if ( (*( p)) >= -62 )
			goto st685;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr859;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr859;
		} else
			goto tr859;
	} else
		goto st695;
	goto tr2320;
st686:
	if ( ++( p) == ( pe) )
		goto _test_eof686;
case 686:
	if ( (*( p)) <= -65 )
		goto st685;
	goto tr250;
st687:
	if ( ++( p) == ( pe) )
		goto _test_eof687;
case 687:
	if ( (*( p)) == -99 )
		goto st688;
	if ( (*( p)) <= -65 )
		goto st685;
	goto tr250;
st688:
	if ( ++( p) == ( pe) )
		goto _test_eof688;
case 688:
	if ( (*( p)) == -83 )
		goto st689;
	if ( (*( p)) <= -65 )
		goto tr859;
	goto tr250;
st689:
	if ( ++( p) == ( pe) )
		goto _test_eof689;
case 689:
	switch( (*( p)) ) {
		case -30: goto st687;
		case -29: goto st690;
		case -17: goto st692;
		case 35: goto tr862;
		case 46: goto st684;
		case 47: goto tr863;
		case 58: goto st729;
		case 63: goto st718;
		case 95: goto tr859;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st686;
		} else if ( (*( p)) >= -62 )
			goto st685;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 65 ) {
			if ( 45 <= (*( p)) && (*( p)) <= 57 )
				goto tr859;
		} else if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr859;
		} else
			goto tr859;
	} else
		goto st695;
	goto tr250;
st690:
	if ( ++( p) == ( pe) )
		goto _test_eof690;
case 690:
	if ( (*( p)) == -128 )
		goto st691;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st685;
	goto tr250;
st691:
	if ( ++( p) == ( pe) )
		goto _test_eof691;
case 691:
	if ( (*( p)) < -120 ) {
		if ( (*( p)) > -126 ) {
			if ( -125 <= (*( p)) && (*( p)) <= -121 )
				goto tr859;
		} else
			goto st689;
	} else if ( (*( p)) > -111 ) {
		if ( (*( p)) < -108 ) {
			if ( -110 <= (*( p)) && (*( p)) <= -109 )
				goto tr859;
		} else if ( (*( p)) > -100 ) {
			if ( -99 <= (*( p)) && (*( p)) <= -65 )
				goto tr859;
		} else
			goto st689;
	} else
		goto st689;
	goto tr250;
st692:
	if ( ++( p) == ( pe) )
		goto _test_eof692;
case 692:
	switch( (*( p)) ) {
		case -68: goto st693;
		case -67: goto st694;
	}
	if ( (*( p)) <= -65 )
		goto st685;
	goto tr250;
st693:
	if ( ++( p) == ( pe) )
		goto _test_eof693;
case 693:
	switch( (*( p)) ) {
		case -119: goto st689;
		case -67: goto st689;
	}
	if ( (*( p)) <= -65 )
		goto tr859;
	goto tr250;
st694:
	if ( ++( p) == ( pe) )
		goto _test_eof694;
case 694:
	switch( (*( p)) ) {
		case -99: goto st689;
		case -96: goto st689;
		case -93: goto st689;
	}
	if ( (*( p)) <= -65 )
		goto tr859;
	goto tr250;
st695:
	if ( ++( p) == ( pe) )
		goto _test_eof695;
case 695:
	if ( (*( p)) <= -65 )
		goto st686;
	goto tr250;
tr862:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1809;
st1809:
	if ( ++( p) == ( pe) )
		goto _test_eof1809;
case 1809:
#line 16031 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st698;
		case -29: goto st700;
		case -17: goto st702;
		case 32: goto tr2320;
		case 34: goto st706;
		case 35: goto tr2320;
		case 39: goto st706;
		case 44: goto st706;
		case 46: goto st706;
		case 60: goto tr2320;
		case 62: goto tr2320;
		case 63: goto st706;
		case 91: goto tr2320;
		case 93: goto tr2320;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2320;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st697;
		} else
			goto st696;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2320;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st706;
		} else
			goto tr2320;
	} else
		goto st705;
	goto tr862;
st696:
	if ( ++( p) == ( pe) )
		goto _test_eof696;
case 696:
	if ( (*( p)) <= -65 )
		goto tr862;
	goto tr869;
st697:
	if ( ++( p) == ( pe) )
		goto _test_eof697;
case 697:
	if ( (*( p)) <= -65 )
		goto st696;
	goto tr869;
st698:
	if ( ++( p) == ( pe) )
		goto _test_eof698;
case 698:
	if ( (*( p)) == -99 )
		goto st699;
	if ( (*( p)) <= -65 )
		goto st696;
	goto tr869;
st699:
	if ( ++( p) == ( pe) )
		goto _test_eof699;
case 699:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr862;
	} else
		goto tr862;
	goto tr869;
st700:
	if ( ++( p) == ( pe) )
		goto _test_eof700;
case 700:
	if ( (*( p)) == -128 )
		goto st701;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st696;
	goto tr869;
st701:
	if ( ++( p) == ( pe) )
		goto _test_eof701;
case 701:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr862;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr862;
	} else
		goto tr862;
	goto tr869;
st702:
	if ( ++( p) == ( pe) )
		goto _test_eof702;
case 702:
	switch( (*( p)) ) {
		case -68: goto st703;
		case -67: goto st704;
	}
	if ( (*( p)) <= -65 )
		goto st696;
	goto tr869;
st703:
	if ( ++( p) == ( pe) )
		goto _test_eof703;
case 703:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr862;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr862;
	} else
		goto tr862;
	goto tr869;
st704:
	if ( ++( p) == ( pe) )
		goto _test_eof704;
case 704:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr862;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr862;
		} else if ( (*( p)) >= -95 )
			goto tr862;
	} else
		goto tr862;
	goto tr869;
st705:
	if ( ++( p) == ( pe) )
		goto _test_eof705;
case 705:
	if ( (*( p)) <= -65 )
		goto st697;
	goto tr869;
st706:
	if ( ++( p) == ( pe) )
		goto _test_eof706;
case 706:
	switch( (*( p)) ) {
		case -30: goto st698;
		case -29: goto st700;
		case -17: goto st702;
		case 32: goto tr869;
		case 34: goto st706;
		case 35: goto tr869;
		case 39: goto st706;
		case 44: goto st706;
		case 46: goto st706;
		case 60: goto tr869;
		case 62: goto tr869;
		case 63: goto st706;
		case 91: goto tr869;
		case 93: goto tr869;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr869;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st697;
		} else
			goto st696;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr869;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st706;
		} else
			goto tr869;
	} else
		goto st705;
	goto tr862;
tr863:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 410 "ext/dtext/dtext.cpp.rl"
	{( act) = 56;}
	goto st1810;
st1810:
	if ( ++( p) == ( pe) )
		goto _test_eof1810;
case 1810:
#line 16222 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st709;
		case -29: goto st711;
		case -17: goto st713;
		case 32: goto tr2320;
		case 34: goto st717;
		case 35: goto tr862;
		case 39: goto st717;
		case 44: goto st717;
		case 46: goto st717;
		case 60: goto tr2320;
		case 62: goto tr2320;
		case 63: goto st718;
		case 91: goto tr2320;
		case 93: goto tr2320;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2320;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st708;
		} else
			goto st707;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2320;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st717;
		} else
			goto tr2320;
	} else
		goto st716;
	goto tr863;
st707:
	if ( ++( p) == ( pe) )
		goto _test_eof707;
case 707:
	if ( (*( p)) <= -65 )
		goto tr863;
	goto tr869;
st708:
	if ( ++( p) == ( pe) )
		goto _test_eof708;
case 708:
	if ( (*( p)) <= -65 )
		goto st707;
	goto tr869;
st709:
	if ( ++( p) == ( pe) )
		goto _test_eof709;
case 709:
	if ( (*( p)) == -99 )
		goto st710;
	if ( (*( p)) <= -65 )
		goto st707;
	goto tr869;
st710:
	if ( ++( p) == ( pe) )
		goto _test_eof710;
case 710:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr863;
	} else
		goto tr863;
	goto tr869;
st711:
	if ( ++( p) == ( pe) )
		goto _test_eof711;
case 711:
	if ( (*( p)) == -128 )
		goto st712;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st707;
	goto tr869;
st712:
	if ( ++( p) == ( pe) )
		goto _test_eof712;
case 712:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr863;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr863;
	} else
		goto tr863;
	goto tr869;
st713:
	if ( ++( p) == ( pe) )
		goto _test_eof713;
case 713:
	switch( (*( p)) ) {
		case -68: goto st714;
		case -67: goto st715;
	}
	if ( (*( p)) <= -65 )
		goto st707;
	goto tr869;
st714:
	if ( ++( p) == ( pe) )
		goto _test_eof714;
case 714:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr863;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr863;
	} else
		goto tr863;
	goto tr869;
st715:
	if ( ++( p) == ( pe) )
		goto _test_eof715;
case 715:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr863;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr863;
		} else if ( (*( p)) >= -95 )
			goto tr863;
	} else
		goto tr863;
	goto tr869;
st716:
	if ( ++( p) == ( pe) )
		goto _test_eof716;
case 716:
	if ( (*( p)) <= -65 )
		goto st708;
	goto tr869;
st717:
	if ( ++( p) == ( pe) )
		goto _test_eof717;
case 717:
	switch( (*( p)) ) {
		case -30: goto st709;
		case -29: goto st711;
		case -17: goto st713;
		case 32: goto tr869;
		case 34: goto st717;
		case 35: goto tr862;
		case 39: goto st717;
		case 44: goto st717;
		case 46: goto st717;
		case 60: goto tr869;
		case 62: goto tr869;
		case 63: goto st718;
		case 91: goto tr869;
		case 93: goto tr869;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr869;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st708;
		} else
			goto st707;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr869;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st717;
		} else
			goto tr869;
	} else
		goto st716;
	goto tr863;
st718:
	if ( ++( p) == ( pe) )
		goto _test_eof718;
case 718:
	switch( (*( p)) ) {
		case -30: goto st721;
		case -29: goto st723;
		case -17: goto st725;
		case 32: goto tr250;
		case 34: goto st718;
		case 35: goto tr862;
		case 39: goto st718;
		case 44: goto st718;
		case 46: goto st718;
		case 63: goto st718;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr250;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st720;
		} else
			goto st719;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr250;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st718;
		} else
			goto tr250;
	} else
		goto st728;
	goto tr898;
st719:
	if ( ++( p) == ( pe) )
		goto _test_eof719;
case 719:
	if ( (*( p)) <= -65 )
		goto tr898;
	goto tr250;
tr898:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 410 "ext/dtext/dtext.cpp.rl"
	{( act) = 56;}
	goto st1811;
st1811:
	if ( ++( p) == ( pe) )
		goto _test_eof1811;
case 1811:
#line 16457 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case -30: goto st721;
		case -29: goto st723;
		case -17: goto st725;
		case 32: goto tr2320;
		case 34: goto st718;
		case 35: goto tr862;
		case 39: goto st718;
		case 44: goto st718;
		case 46: goto st718;
		case 63: goto st718;
	}
	if ( (*( p)) < -16 ) {
		if ( (*( p)) < -62 ) {
			if ( (*( p)) <= -63 )
				goto tr2320;
		} else if ( (*( p)) > -33 ) {
			if ( -32 <= (*( p)) && (*( p)) <= -18 )
				goto st720;
		} else
			goto st719;
	} else if ( (*( p)) > -12 ) {
		if ( (*( p)) < 9 ) {
			if ( -11 <= (*( p)) && (*( p)) <= 0 )
				goto tr2320;
		} else if ( (*( p)) > 13 ) {
			if ( 58 <= (*( p)) && (*( p)) <= 59 )
				goto st718;
		} else
			goto tr2320;
	} else
		goto st728;
	goto tr898;
st720:
	if ( ++( p) == ( pe) )
		goto _test_eof720;
case 720:
	if ( (*( p)) <= -65 )
		goto st719;
	goto tr250;
st721:
	if ( ++( p) == ( pe) )
		goto _test_eof721;
case 721:
	if ( (*( p)) == -99 )
		goto st722;
	if ( (*( p)) <= -65 )
		goto st719;
	goto tr250;
st722:
	if ( ++( p) == ( pe) )
		goto _test_eof722;
case 722:
	if ( (*( p)) > -84 ) {
		if ( -82 <= (*( p)) && (*( p)) <= -65 )
			goto tr898;
	} else
		goto tr898;
	goto tr250;
st723:
	if ( ++( p) == ( pe) )
		goto _test_eof723;
case 723:
	if ( (*( p)) == -128 )
		goto st724;
	if ( -127 <= (*( p)) && (*( p)) <= -65 )
		goto st719;
	goto tr250;
st724:
	if ( ++( p) == ( pe) )
		goto _test_eof724;
case 724:
	if ( (*( p)) < -110 ) {
		if ( -125 <= (*( p)) && (*( p)) <= -121 )
			goto tr898;
	} else if ( (*( p)) > -109 ) {
		if ( -99 <= (*( p)) && (*( p)) <= -65 )
			goto tr898;
	} else
		goto tr898;
	goto tr250;
st725:
	if ( ++( p) == ( pe) )
		goto _test_eof725;
case 725:
	switch( (*( p)) ) {
		case -68: goto st726;
		case -67: goto st727;
	}
	if ( (*( p)) <= -65 )
		goto st719;
	goto tr250;
st726:
	if ( ++( p) == ( pe) )
		goto _test_eof726;
case 726:
	if ( (*( p)) < -118 ) {
		if ( (*( p)) <= -120 )
			goto tr898;
	} else if ( (*( p)) > -68 ) {
		if ( -66 <= (*( p)) && (*( p)) <= -65 )
			goto tr898;
	} else
		goto tr898;
	goto tr250;
st727:
	if ( ++( p) == ( pe) )
		goto _test_eof727;
case 727:
	if ( (*( p)) < -98 ) {
		if ( (*( p)) <= -100 )
			goto tr898;
	} else if ( (*( p)) > -97 ) {
		if ( (*( p)) > -94 ) {
			if ( -92 <= (*( p)) && (*( p)) <= -65 )
				goto tr898;
		} else if ( (*( p)) >= -95 )
			goto tr898;
	} else
		goto tr898;
	goto tr250;
st728:
	if ( ++( p) == ( pe) )
		goto _test_eof728;
case 728:
	if ( (*( p)) <= -65 )
		goto st720;
	goto tr250;
st729:
	if ( ++( p) == ( pe) )
		goto _test_eof729;
case 729:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr903;
	goto tr250;
tr903:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 410 "ext/dtext/dtext.cpp.rl"
	{( act) = 56;}
	goto st1812;
st1812:
	if ( ++( p) == ( pe) )
		goto _test_eof1812;
case 1812:
#line 16603 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 35: goto tr862;
		case 47: goto tr863;
		case 63: goto st718;
	}
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr903;
	goto tr2320;
tr2319:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1813;
st1813:
	if ( ++( p) == ( pe) )
		goto _test_eof1813;
case 1813:
#line 16622 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 58: goto st677;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2155:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1814;
st1814:
	if ( ++( p) == ( pe) )
		goto _test_eof1814;
case 1814:
#line 16649 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2321;
		case 83: goto tr2322;
		case 91: goto tr2199;
		case 109: goto tr2321;
		case 115: goto tr2322;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2321:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1815;
st1815:
	if ( ++( p) == ( pe) )
		goto _test_eof1815;
case 1815:
#line 16677 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2323;
		case 91: goto tr2199;
		case 112: goto tr2323;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2323:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1816;
st1816:
	if ( ++( p) == ( pe) )
		goto _test_eof1816;
case 1816:
#line 16703 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2324;
		case 91: goto tr2199;
		case 108: goto tr2324;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2324:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1817;
st1817:
	if ( ++( p) == ( pe) )
		goto _test_eof1817;
case 1817:
#line 16729 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2325;
		case 91: goto tr2199;
		case 105: goto tr2325;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2325:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1818;
st1818:
	if ( ++( p) == ( pe) )
		goto _test_eof1818;
case 1818:
#line 16755 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2326;
		case 91: goto tr2199;
		case 99: goto tr2326;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2326:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1819;
st1819:
	if ( ++( p) == ( pe) )
		goto _test_eof1819;
case 1819:
#line 16781 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2327;
		case 91: goto tr2199;
		case 97: goto tr2327;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 16807 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2328;
		case 91: goto tr2199;
		case 116: goto tr2328;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2328:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1821;
st1821:
	if ( ++( p) == ( pe) )
		goto _test_eof1821;
case 1821:
#line 16833 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2329;
		case 91: goto tr2199;
		case 105: goto tr2329;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 16859 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2330;
		case 91: goto tr2199;
		case 111: goto tr2330;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2330:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1823;
st1823:
	if ( ++( p) == ( pe) )
		goto _test_eof1823;
case 1823:
#line 16885 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2331;
		case 91: goto tr2199;
		case 110: goto tr2331;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2331:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1824;
st1824:
	if ( ++( p) == ( pe) )
		goto _test_eof1824;
case 1824:
#line 16911 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st730;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st730:
	if ( ++( p) == ( pe) )
		goto _test_eof730;
case 730:
	if ( (*( p)) == 35 )
		goto st731;
	goto tr253;
st731:
	if ( ++( p) == ( pe) )
		goto _test_eof731;
case 731:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr905;
	goto tr253;
tr905:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1825;
st1825:
	if ( ++( p) == ( pe) )
		goto _test_eof1825;
case 1825:
#line 16948 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1825;
	goto tr2333;
tr2322:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1826;
st1826:
	if ( ++( p) == ( pe) )
		goto _test_eof1826;
case 1826:
#line 16962 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2335;
		case 91: goto tr2199;
		case 115: goto tr2335;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 16988 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2336;
		case 91: goto tr2199;
		case 117: goto tr2336;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 17014 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2337;
		case 91: goto tr2199;
		case 101: goto tr2337;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 17040 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st732;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st732:
	if ( ++( p) == ( pe) )
		goto _test_eof732;
case 732:
	if ( (*( p)) == 35 )
		goto st733;
	goto tr253;
st733:
	if ( ++( p) == ( pe) )
		goto _test_eof733;
case 733:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr907;
	goto tr253;
tr907:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1830;
st1830:
	if ( ++( p) == ( pe) )
		goto _test_eof1830;
case 1830:
#line 17077 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1830;
	goto tr2339;
tr2156:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1831;
st1831:
	if ( ++( p) == ( pe) )
		goto _test_eof1831;
case 1831:
#line 17093 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2341;
		case 91: goto tr2199;
		case 111: goto tr2341;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2341:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1832;
st1832:
	if ( ++( p) == ( pe) )
		goto _test_eof1832;
case 1832:
#line 17119 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2342;
		case 91: goto tr2199;
		case 100: goto tr2342;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2342:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1833;
st1833:
	if ( ++( p) == ( pe) )
		goto _test_eof1833;
case 1833:
#line 17145 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st734;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st734:
	if ( ++( p) == ( pe) )
		goto _test_eof734;
case 734:
	switch( (*( p)) ) {
		case 65: goto st735;
		case 97: goto st735;
	}
	goto tr253;
st735:
	if ( ++( p) == ( pe) )
		goto _test_eof735;
case 735:
	switch( (*( p)) ) {
		case 67: goto st736;
		case 99: goto st736;
	}
	goto tr253;
st736:
	if ( ++( p) == ( pe) )
		goto _test_eof736;
case 736:
	switch( (*( p)) ) {
		case 84: goto st737;
		case 116: goto st737;
	}
	goto tr253;
st737:
	if ( ++( p) == ( pe) )
		goto _test_eof737;
case 737:
	switch( (*( p)) ) {
		case 73: goto st738;
		case 105: goto st738;
	}
	goto tr253;
st738:
	if ( ++( p) == ( pe) )
		goto _test_eof738;
case 738:
	switch( (*( p)) ) {
		case 79: goto st739;
		case 111: goto st739;
	}
	goto tr253;
st739:
	if ( ++( p) == ( pe) )
		goto _test_eof739;
case 739:
	switch( (*( p)) ) {
		case 78: goto st740;
		case 110: goto st740;
	}
	goto tr253;
st740:
	if ( ++( p) == ( pe) )
		goto _test_eof740;
case 740:
	if ( (*( p)) == 32 )
		goto st741;
	goto tr253;
st741:
	if ( ++( p) == ( pe) )
		goto _test_eof741;
case 741:
	if ( (*( p)) == 35 )
		goto st742;
	goto tr253;
st742:
	if ( ++( p) == ( pe) )
		goto _test_eof742;
case 742:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr916;
	goto tr253;
tr916:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1834;
st1834:
	if ( ++( p) == ( pe) )
		goto _test_eof1834;
case 1834:
#line 17243 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1834;
	goto tr2344;
tr2157:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1835;
st1835:
	if ( ++( p) == ( pe) )
		goto _test_eof1835;
case 1835:
#line 17259 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2346;
		case 91: goto tr2199;
		case 111: goto tr2346;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2346:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1836;
st1836:
	if ( ++( p) == ( pe) )
		goto _test_eof1836;
case 1836:
#line 17285 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2347;
		case 91: goto tr2199;
		case 116: goto tr2347;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2347:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1837;
st1837:
	if ( ++( p) == ( pe) )
		goto _test_eof1837;
case 1837:
#line 17311 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2348;
		case 91: goto tr2199;
		case 101: goto tr2348;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2348:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1838;
st1838:
	if ( ++( p) == ( pe) )
		goto _test_eof1838;
case 1838:
#line 17337 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st743;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st743:
	if ( ++( p) == ( pe) )
		goto _test_eof743;
case 743:
	if ( (*( p)) == 35 )
		goto st744;
	goto tr253;
st744:
	if ( ++( p) == ( pe) )
		goto _test_eof744;
case 744:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr918;
	goto tr253;
tr918:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1839;
st1839:
	if ( ++( p) == ( pe) )
		goto _test_eof1839;
case 1839:
#line 17374 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1839;
	goto tr2350;
tr2158:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1840;
st1840:
	if ( ++( p) == ( pe) )
		goto _test_eof1840;
case 1840:
#line 17390 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2352;
		case 85: goto tr2353;
		case 91: goto tr2199;
		case 111: goto tr2352;
		case 117: goto tr2353;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2352:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1841;
st1841:
	if ( ++( p) == ( pe) )
		goto _test_eof1841;
case 1841:
#line 17418 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2354;
		case 83: goto tr2355;
		case 91: goto tr2199;
		case 111: goto tr2354;
		case 115: goto tr2355;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2354:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1842;
st1842:
	if ( ++( p) == ( pe) )
		goto _test_eof1842;
case 1842:
#line 17446 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2356;
		case 91: goto tr2199;
		case 108: goto tr2356;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2356:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1843;
st1843:
	if ( ++( p) == ( pe) )
		goto _test_eof1843;
case 1843:
#line 17472 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st745;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st745:
	if ( ++( p) == ( pe) )
		goto _test_eof745;
case 745:
	if ( (*( p)) == 35 )
		goto st746;
	goto tr253;
st746:
	if ( ++( p) == ( pe) )
		goto _test_eof746;
case 746:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr920;
	goto tr253;
tr920:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1844;
st1844:
	if ( ++( p) == ( pe) )
		goto _test_eof1844;
case 1844:
#line 17509 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1844;
	goto tr2358;
tr2355:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1845;
st1845:
	if ( ++( p) == ( pe) )
		goto _test_eof1845;
case 1845:
#line 17523 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2360;
		case 91: goto tr2199;
		case 116: goto tr2360;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2360:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1846;
st1846:
	if ( ++( p) == ( pe) )
		goto _test_eof1846;
case 1846:
#line 17549 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st747;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st747:
	if ( ++( p) == ( pe) )
		goto _test_eof747;
case 747:
	switch( (*( p)) ) {
		case 35: goto st748;
		case 67: goto st749;
		case 99: goto st749;
	}
	goto tr253;
st748:
	if ( ++( p) == ( pe) )
		goto _test_eof748;
case 748:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr923;
	goto tr253;
tr923:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1847;
st1847:
	if ( ++( p) == ( pe) )
		goto _test_eof1847;
case 1847:
#line 17589 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1847;
	goto tr2362;
st749:
	if ( ++( p) == ( pe) )
		goto _test_eof749;
case 749:
	switch( (*( p)) ) {
		case 72: goto st750;
		case 104: goto st750;
	}
	goto tr253;
st750:
	if ( ++( p) == ( pe) )
		goto _test_eof750;
case 750:
	switch( (*( p)) ) {
		case 65: goto st751;
		case 97: goto st751;
	}
	goto tr253;
st751:
	if ( ++( p) == ( pe) )
		goto _test_eof751;
case 751:
	switch( (*( p)) ) {
		case 78: goto st752;
		case 110: goto st752;
	}
	goto tr253;
st752:
	if ( ++( p) == ( pe) )
		goto _test_eof752;
case 752:
	switch( (*( p)) ) {
		case 71: goto st753;
		case 103: goto st753;
	}
	goto tr253;
st753:
	if ( ++( p) == ( pe) )
		goto _test_eof753;
case 753:
	switch( (*( p)) ) {
		case 69: goto st754;
		case 101: goto st754;
	}
	goto tr253;
st754:
	if ( ++( p) == ( pe) )
		goto _test_eof754;
case 754:
	switch( (*( p)) ) {
		case 83: goto st755;
		case 115: goto st755;
	}
	goto tr253;
st755:
	if ( ++( p) == ( pe) )
		goto _test_eof755;
case 755:
	if ( (*( p)) == 32 )
		goto st756;
	goto tr253;
st756:
	if ( ++( p) == ( pe) )
		goto _test_eof756;
case 756:
	if ( (*( p)) == 35 )
		goto st757;
	goto tr253;
st757:
	if ( ++( p) == ( pe) )
		goto _test_eof757;
case 757:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr932;
	goto tr253;
tr932:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1848;
st1848:
	if ( ++( p) == ( pe) )
		goto _test_eof1848;
case 1848:
#line 17676 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1848;
	goto tr2364;
tr2353:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1849;
st1849:
	if ( ++( p) == ( pe) )
		goto _test_eof1849;
case 1849:
#line 17690 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2366;
		case 91: goto tr2199;
		case 108: goto tr2366;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2366:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1850;
st1850:
	if ( ++( p) == ( pe) )
		goto _test_eof1850;
case 1850:
#line 17716 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 76: goto tr2367;
		case 91: goto tr2199;
		case 108: goto tr2367;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2367:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1851;
st1851:
	if ( ++( p) == ( pe) )
		goto _test_eof1851;
case 1851:
#line 17742 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st758;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st758:
	if ( ++( p) == ( pe) )
		goto _test_eof758;
case 758:
	if ( (*( p)) == 35 )
		goto st759;
	goto tr253;
st759:
	if ( ++( p) == ( pe) )
		goto _test_eof759;
case 759:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr934;
	goto tr253;
tr934:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1852;
st1852:
	if ( ++( p) == ( pe) )
		goto _test_eof1852;
case 1852:
#line 17779 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1852;
	goto tr2369;
tr2159:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1853;
st1853:
	if ( ++( p) == ( pe) )
		goto _test_eof1853;
case 1853:
#line 17795 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2371;
		case 91: goto tr2199;
		case 101: goto tr2371;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2371:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1854;
st1854:
	if ( ++( p) == ( pe) )
		goto _test_eof1854;
case 1854:
#line 17821 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2372;
		case 91: goto tr2199;
		case 99: goto tr2372;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2372:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1855;
st1855:
	if ( ++( p) == ( pe) )
		goto _test_eof1855;
case 1855:
#line 17847 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2373;
		case 91: goto tr2199;
		case 111: goto tr2373;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2373:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1856;
st1856:
	if ( ++( p) == ( pe) )
		goto _test_eof1856;
case 1856:
#line 17873 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2374;
		case 91: goto tr2199;
		case 114: goto tr2374;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 17899 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 68: goto tr2375;
		case 91: goto tr2199;
		case 100: goto tr2375;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 17925 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st760;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st760:
	if ( ++( p) == ( pe) )
		goto _test_eof760;
case 760:
	if ( (*( p)) == 35 )
		goto st761;
	goto tr253;
st761:
	if ( ++( p) == ( pe) )
		goto _test_eof761;
case 761:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr936;
	goto tr253;
tr936:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1859;
st1859:
	if ( ++( p) == ( pe) )
		goto _test_eof1859;
case 1859:
#line 17962 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1859;
	goto tr2377;
tr2160:
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
#line 17978 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2379;
		case 91: goto tr2199;
		case 101: goto tr2379;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 18004 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2380;
		case 91: goto tr2199;
		case 116: goto tr2380;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 18030 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st762;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st762:
	if ( ++( p) == ( pe) )
		goto _test_eof762;
case 762:
	if ( (*( p)) == 35 )
		goto st763;
	goto tr253;
st763:
	if ( ++( p) == ( pe) )
		goto _test_eof763;
case 763:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr938;
	goto tr253;
tr938:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1863;
st1863:
	if ( ++( p) == ( pe) )
		goto _test_eof1863;
case 1863:
#line 18067 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1863;
	goto tr2382;
tr2161:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1864;
st1864:
	if ( ++( p) == ( pe) )
		goto _test_eof1864;
case 1864:
#line 18083 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2384;
		case 72: goto tr2385;
		case 73: goto tr2386;
		case 79: goto tr2387;
		case 91: goto tr2199;
		case 97: goto tr2384;
		case 104: goto tr2385;
		case 105: goto tr2386;
		case 111: goto tr2387;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2384:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1865;
st1865:
	if ( ++( p) == ( pe) )
		goto _test_eof1865;
case 1865:
#line 18115 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2388;
		case 91: goto tr2199;
		case 107: goto tr2388;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2388:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1866;
st1866:
	if ( ++( p) == ( pe) )
		goto _test_eof1866;
case 1866:
#line 18141 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2389;
		case 91: goto tr2199;
		case 101: goto tr2389;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2389:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1867;
st1867:
	if ( ++( p) == ( pe) )
		goto _test_eof1867;
case 1867:
#line 18167 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st764;
		case 68: goto tr2391;
		case 91: goto tr2199;
		case 100: goto tr2391;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st764:
	if ( ++( p) == ( pe) )
		goto _test_eof764;
case 764:
	switch( (*( p)) ) {
		case 68: goto st765;
		case 100: goto st765;
	}
	goto tr253;
st765:
	if ( ++( p) == ( pe) )
		goto _test_eof765;
case 765:
	switch( (*( p)) ) {
		case 79: goto st766;
		case 111: goto st766;
	}
	goto tr253;
st766:
	if ( ++( p) == ( pe) )
		goto _test_eof766;
case 766:
	switch( (*( p)) ) {
		case 87: goto st767;
		case 119: goto st767;
	}
	goto tr253;
st767:
	if ( ++( p) == ( pe) )
		goto _test_eof767;
case 767:
	switch( (*( p)) ) {
		case 78: goto st768;
		case 110: goto st768;
	}
	goto tr253;
st768:
	if ( ++( p) == ( pe) )
		goto _test_eof768;
case 768:
	if ( (*( p)) == 32 )
		goto st769;
	goto tr253;
st769:
	if ( ++( p) == ( pe) )
		goto _test_eof769;
case 769:
	switch( (*( p)) ) {
		case 35: goto st770;
		case 82: goto st771;
		case 114: goto st771;
	}
	goto tr253;
st770:
	if ( ++( p) == ( pe) )
		goto _test_eof770;
case 770:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr946;
	goto tr253;
tr946:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1868;
st1868:
	if ( ++( p) == ( pe) )
		goto _test_eof1868;
case 1868:
#line 18252 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1868;
	goto tr2392;
st771:
	if ( ++( p) == ( pe) )
		goto _test_eof771;
case 771:
	switch( (*( p)) ) {
		case 69: goto st772;
		case 101: goto st772;
	}
	goto tr253;
st772:
	if ( ++( p) == ( pe) )
		goto _test_eof772;
case 772:
	switch( (*( p)) ) {
		case 81: goto st773;
		case 113: goto st773;
	}
	goto tr253;
st773:
	if ( ++( p) == ( pe) )
		goto _test_eof773;
case 773:
	switch( (*( p)) ) {
		case 85: goto st774;
		case 117: goto st774;
	}
	goto tr253;
st774:
	if ( ++( p) == ( pe) )
		goto _test_eof774;
case 774:
	switch( (*( p)) ) {
		case 69: goto st775;
		case 101: goto st775;
	}
	goto tr253;
st775:
	if ( ++( p) == ( pe) )
		goto _test_eof775;
case 775:
	switch( (*( p)) ) {
		case 83: goto st776;
		case 115: goto st776;
	}
	goto tr253;
st776:
	if ( ++( p) == ( pe) )
		goto _test_eof776;
case 776:
	switch( (*( p)) ) {
		case 84: goto st777;
		case 116: goto st777;
	}
	goto tr253;
st777:
	if ( ++( p) == ( pe) )
		goto _test_eof777;
case 777:
	if ( (*( p)) == 32 )
		goto st778;
	goto tr253;
st778:
	if ( ++( p) == ( pe) )
		goto _test_eof778;
case 778:
	if ( (*( p)) == 35 )
		goto st770;
	goto tr253;
tr2391:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1869;
st1869:
	if ( ++( p) == ( pe) )
		goto _test_eof1869;
case 1869:
#line 18334 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 79: goto tr2394;
		case 91: goto tr2199;
		case 111: goto tr2394;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2394:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1870;
st1870:
	if ( ++( p) == ( pe) )
		goto _test_eof1870;
case 1870:
#line 18360 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 87: goto tr2395;
		case 91: goto tr2199;
		case 119: goto tr2395;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2395:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1871;
st1871:
	if ( ++( p) == ( pe) )
		goto _test_eof1871;
case 1871:
#line 18386 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2396;
		case 91: goto tr2199;
		case 110: goto tr2396;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2396:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1872;
st1872:
	if ( ++( p) == ( pe) )
		goto _test_eof1872;
case 1872:
#line 18412 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st769;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2385:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1873;
st1873:
	if ( ++( p) == ( pe) )
		goto _test_eof1873;
case 1873:
#line 18437 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 85: goto tr2397;
		case 91: goto tr2199;
		case 117: goto tr2397;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 18463 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 77: goto tr2398;
		case 91: goto tr2199;
		case 109: goto tr2398;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2398:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1875;
st1875:
	if ( ++( p) == ( pe) )
		goto _test_eof1875;
case 1875:
#line 18489 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 66: goto tr2399;
		case 91: goto tr2199;
		case 98: goto tr2399;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 18515 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st779;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st779:
	if ( ++( p) == ( pe) )
		goto _test_eof779;
case 779:
	if ( (*( p)) == 35 )
		goto st780;
	goto tr253;
st780:
	if ( ++( p) == ( pe) )
		goto _test_eof780;
case 780:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr955;
	goto tr253;
tr955:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1877;
st1877:
	if ( ++( p) == ( pe) )
		goto _test_eof1877;
case 1877:
#line 18552 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1877;
	goto tr2401;
tr2386:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1878;
st1878:
	if ( ++( p) == ( pe) )
		goto _test_eof1878;
case 1878:
#line 18566 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2403;
		case 91: goto tr2199;
		case 99: goto tr2403;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2403:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1879;
st1879:
	if ( ++( p) == ( pe) )
		goto _test_eof1879;
case 1879:
#line 18592 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2404;
		case 91: goto tr2199;
		case 107: goto tr2404;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2404:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1880;
st1880:
	if ( ++( p) == ( pe) )
		goto _test_eof1880;
case 1880:
#line 18618 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2405;
		case 91: goto tr2199;
		case 101: goto tr2405;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 18644 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto tr2406;
		case 91: goto tr2199;
		case 116: goto tr2406;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 18670 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st781;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st781:
	if ( ++( p) == ( pe) )
		goto _test_eof781;
case 781:
	if ( (*( p)) == 35 )
		goto st782;
	goto tr253;
st782:
	if ( ++( p) == ( pe) )
		goto _test_eof782;
case 782:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr957;
	goto tr253;
tr957:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1883;
st1883:
	if ( ++( p) == ( pe) )
		goto _test_eof1883;
case 1883:
#line 18707 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1883;
	goto tr2408;
tr2387:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1884;
st1884:
	if ( ++( p) == ( pe) )
		goto _test_eof1884;
case 1884:
#line 18721 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 80: goto tr2410;
		case 91: goto tr2199;
		case 112: goto tr2410;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2410:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1885;
st1885:
	if ( ++( p) == ( pe) )
		goto _test_eof1885;
case 1885:
#line 18747 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2411;
		case 91: goto tr2199;
		case 105: goto tr2411;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 18773 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 67: goto tr2412;
		case 91: goto tr2199;
		case 99: goto tr2412;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 18799 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st673;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2162:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1888;
st1888:
	if ( ++( p) == ( pe) )
		goto _test_eof1888;
case 1888:
#line 18826 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2413;
		case 91: goto tr2199;
		case 115: goto tr2413;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2413:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1889;
st1889:
	if ( ++( p) == ( pe) )
		goto _test_eof1889;
case 1889:
#line 18852 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2414;
		case 91: goto tr2199;
		case 101: goto tr2414;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2414:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1890;
st1890:
	if ( ++( p) == ( pe) )
		goto _test_eof1890;
case 1890:
#line 18878 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 82: goto tr2415;
		case 91: goto tr2199;
		case 114: goto tr2415;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2415:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1891;
st1891:
	if ( ++( p) == ( pe) )
		goto _test_eof1891;
case 1891:
#line 18904 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st783;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st783:
	if ( ++( p) == ( pe) )
		goto _test_eof783;
case 783:
	if ( (*( p)) == 35 )
		goto st784;
	goto tr253;
st784:
	if ( ++( p) == ( pe) )
		goto _test_eof784;
case 784:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr959;
	goto tr253;
tr959:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1892;
st1892:
	if ( ++( p) == ( pe) )
		goto _test_eof1892;
case 1892:
#line 18941 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1892;
	goto tr2417;
tr2163:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1893;
st1893:
	if ( ++( p) == ( pe) )
		goto _test_eof1893;
case 1893:
#line 18957 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2419;
		case 91: goto tr2199;
		case 105: goto tr2419;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2419:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1894;
st1894:
	if ( ++( p) == ( pe) )
		goto _test_eof1894;
case 1894:
#line 18983 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 75: goto tr2420;
		case 91: goto tr2199;
		case 107: goto tr2420;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2420:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1895;
st1895:
	if ( ++( p) == ( pe) )
		goto _test_eof1895;
case 1895:
#line 19009 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 73: goto tr2421;
		case 91: goto tr2199;
		case 105: goto tr2421;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
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
#line 19035 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st785;
		case 67: goto tr2423;
		case 80: goto tr2424;
		case 91: goto tr2199;
		case 99: goto tr2423;
		case 112: goto tr2424;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
st785:
	if ( ++( p) == ( pe) )
		goto _test_eof785;
case 785:
	switch( (*( p)) ) {
		case 32: goto st786;
		case 35: goto st787;
		case 67: goto st788;
		case 80: goto st797;
		case 99: goto st788;
		case 112: goto st797;
	}
	goto tr253;
st786:
	if ( ++( p) == ( pe) )
		goto _test_eof786;
case 786:
	if ( (*( p)) == 35 )
		goto st787;
	goto tr253;
st787:
	if ( ++( p) == ( pe) )
		goto _test_eof787;
case 787:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr964;
	goto tr253;
tr964:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1897;
st1897:
	if ( ++( p) == ( pe) )
		goto _test_eof1897;
case 1897:
#line 19089 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1897;
	goto tr2425;
st788:
	if ( ++( p) == ( pe) )
		goto _test_eof788;
case 788:
	switch( (*( p)) ) {
		case 72: goto st789;
		case 104: goto st789;
	}
	goto tr253;
st789:
	if ( ++( p) == ( pe) )
		goto _test_eof789;
case 789:
	switch( (*( p)) ) {
		case 65: goto st790;
		case 97: goto st790;
	}
	goto tr253;
st790:
	if ( ++( p) == ( pe) )
		goto _test_eof790;
case 790:
	switch( (*( p)) ) {
		case 78: goto st791;
		case 110: goto st791;
	}
	goto tr253;
st791:
	if ( ++( p) == ( pe) )
		goto _test_eof791;
case 791:
	switch( (*( p)) ) {
		case 71: goto st792;
		case 103: goto st792;
	}
	goto tr253;
st792:
	if ( ++( p) == ( pe) )
		goto _test_eof792;
case 792:
	switch( (*( p)) ) {
		case 69: goto st793;
		case 101: goto st793;
	}
	goto tr253;
st793:
	if ( ++( p) == ( pe) )
		goto _test_eof793;
case 793:
	switch( (*( p)) ) {
		case 83: goto st794;
		case 115: goto st794;
	}
	goto tr253;
st794:
	if ( ++( p) == ( pe) )
		goto _test_eof794;
case 794:
	if ( (*( p)) == 32 )
		goto st795;
	goto tr253;
st795:
	if ( ++( p) == ( pe) )
		goto _test_eof795;
case 795:
	if ( (*( p)) == 35 )
		goto st796;
	goto tr253;
st796:
	if ( ++( p) == ( pe) )
		goto _test_eof796;
case 796:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr973;
	goto tr253;
tr973:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1898;
st1898:
	if ( ++( p) == ( pe) )
		goto _test_eof1898;
case 1898:
#line 19176 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1898;
	goto tr2427;
st797:
	if ( ++( p) == ( pe) )
		goto _test_eof797;
case 797:
	switch( (*( p)) ) {
		case 65: goto st798;
		case 97: goto st798;
	}
	goto tr253;
st798:
	if ( ++( p) == ( pe) )
		goto _test_eof798;
case 798:
	switch( (*( p)) ) {
		case 71: goto st799;
		case 103: goto st799;
	}
	goto tr253;
st799:
	if ( ++( p) == ( pe) )
		goto _test_eof799;
case 799:
	switch( (*( p)) ) {
		case 69: goto st800;
		case 101: goto st800;
	}
	goto tr253;
st800:
	if ( ++( p) == ( pe) )
		goto _test_eof800;
case 800:
	if ( (*( p)) == 32 )
		goto st801;
	goto tr253;
st801:
	if ( ++( p) == ( pe) )
		goto _test_eof801;
case 801:
	switch( (*( p)) ) {
		case 32: goto st795;
		case 35: goto st802;
		case 67: goto st788;
		case 99: goto st788;
	}
	goto tr253;
st802:
	if ( ++( p) == ( pe) )
		goto _test_eof802;
case 802:
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto tr979;
	goto tr253;
tr979:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1899;
st1899:
	if ( ++( p) == ( pe) )
		goto _test_eof1899;
case 1899:
#line 19240 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( p)) && (*( p)) <= 57 )
		goto st1899;
	goto tr2425;
tr2423:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1900;
st1900:
	if ( ++( p) == ( pe) )
		goto _test_eof1900;
case 1900:
#line 19254 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 72: goto tr2430;
		case 91: goto tr2199;
		case 104: goto tr2430;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2430:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1901;
st1901:
	if ( ++( p) == ( pe) )
		goto _test_eof1901;
case 1901:
#line 19280 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2431;
		case 91: goto tr2199;
		case 97: goto tr2431;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2431:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1902;
st1902:
	if ( ++( p) == ( pe) )
		goto _test_eof1902;
case 1902:
#line 19306 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 78: goto tr2432;
		case 91: goto tr2199;
		case 110: goto tr2432;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2432:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1903;
st1903:
	if ( ++( p) == ( pe) )
		goto _test_eof1903;
case 1903:
#line 19332 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2433;
		case 91: goto tr2199;
		case 103: goto tr2433;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2433:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1904;
st1904:
	if ( ++( p) == ( pe) )
		goto _test_eof1904;
case 1904:
#line 19358 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2434;
		case 91: goto tr2199;
		case 101: goto tr2434;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2434:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1905;
st1905:
	if ( ++( p) == ( pe) )
		goto _test_eof1905;
case 1905:
#line 19384 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 83: goto tr2435;
		case 91: goto tr2199;
		case 115: goto tr2435;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2435:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1906;
st1906:
	if ( ++( p) == ( pe) )
		goto _test_eof1906;
case 1906:
#line 19410 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st795;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2424:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1907;
st1907:
	if ( ++( p) == ( pe) )
		goto _test_eof1907;
case 1907:
#line 19435 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 65: goto tr2436;
		case 91: goto tr2199;
		case 97: goto tr2436;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 66 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 98 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2436:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1908;
st1908:
	if ( ++( p) == ( pe) )
		goto _test_eof1908;
case 1908:
#line 19461 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 71: goto tr2437;
		case 91: goto tr2199;
		case 103: goto tr2437;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2437:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1909;
st1909:
	if ( ++( p) == ( pe) )
		goto _test_eof1909;
case 1909:
#line 19487 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 69: goto tr2438;
		case 91: goto tr2199;
		case 101: goto tr2438;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2438:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 625 "ext/dtext/dtext.cpp.rl"
	{( act) = 109;}
	goto st1910;
st1910:
	if ( ++( p) == ( pe) )
		goto _test_eof1910;
case 1910:
#line 19513 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 32: goto st801;
		case 91: goto tr2199;
		case 123: goto tr2200;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2198;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2198;
	} else
		goto tr2198;
	goto tr2170;
tr2164:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1911;
st1911:
	if ( ++( p) == ( pe) )
		goto _test_eof1911;
case 1911:
#line 19542 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2169;
		case 10: goto tr2169;
		case 13: goto tr2169;
		case 35: goto tr2440;
		case 47: goto tr2441;
		case 66: goto tr2442;
		case 67: goto tr2443;
		case 69: goto tr2444;
		case 72: goto tr2445;
		case 73: goto tr2446;
		case 78: goto tr2447;
		case 81: goto tr2448;
		case 83: goto tr2449;
		case 84: goto tr2450;
		case 85: goto tr2451;
		case 91: goto tr2452;
		case 98: goto tr2442;
		case 99: goto tr2443;
		case 101: goto tr2444;
		case 104: goto tr2445;
		case 105: goto tr2446;
		case 110: goto tr2447;
		case 113: goto tr2448;
		case 115: goto tr2449;
		case 116: goto tr2450;
		case 117: goto tr2451;
	}
	goto tr2439;
tr2439:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st803;
st803:
	if ( ++( p) == ( pe) )
		goto _test_eof803;
case 803:
#line 19580 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
	}
	goto st803;
tr981:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st804;
st804:
	if ( ++( p) == ( pe) )
		goto _test_eof804;
case 804:
#line 19596 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st805;
	goto tr257;
st805:
	if ( ++( p) == ( pe) )
		goto _test_eof805;
case 805:
	switch( (*( p)) ) {
		case 35: goto tr983;
		case 47: goto tr983;
		case 72: goto tr984;
		case 104: goto tr984;
	}
	goto tr250;
tr983:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st806;
st806:
	if ( ++( p) == ( pe) )
		goto _test_eof806;
case 806:
#line 19619 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 32: goto tr250;
		case 41: goto tr986;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st806;
tr984:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st807;
st807:
	if ( ++( p) == ( pe) )
		goto _test_eof807;
case 807:
#line 19636 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st808;
		case 116: goto st808;
	}
	goto tr250;
st808:
	if ( ++( p) == ( pe) )
		goto _test_eof808;
case 808:
	switch( (*( p)) ) {
		case 84: goto st809;
		case 116: goto st809;
	}
	goto tr250;
st809:
	if ( ++( p) == ( pe) )
		goto _test_eof809;
case 809:
	switch( (*( p)) ) {
		case 80: goto st810;
		case 112: goto st810;
	}
	goto tr250;
st810:
	if ( ++( p) == ( pe) )
		goto _test_eof810;
case 810:
	switch( (*( p)) ) {
		case 58: goto st811;
		case 83: goto st814;
		case 115: goto st814;
	}
	goto tr250;
st811:
	if ( ++( p) == ( pe) )
		goto _test_eof811;
case 811:
	if ( (*( p)) == 47 )
		goto st812;
	goto tr250;
st812:
	if ( ++( p) == ( pe) )
		goto _test_eof812;
case 812:
	if ( (*( p)) == 47 )
		goto st813;
	goto tr250;
st813:
	if ( ++( p) == ( pe) )
		goto _test_eof813;
case 813:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 32: goto tr250;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st806;
st814:
	if ( ++( p) == ( pe) )
		goto _test_eof814;
case 814:
	if ( (*( p)) == 58 )
		goto st811;
	goto tr250;
tr2440:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st815;
st815:
	if ( ++( p) == ( pe) )
		goto _test_eof815;
case 815:
#line 19712 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 45: goto tr995;
		case 93: goto tr996;
		case 95: goto tr995;
	}
	if ( (*( p)) < 48 ) {
		if ( 9 <= (*( p)) && (*( p)) <= 12 )
			goto st803;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr995;
		} else if ( (*( p)) >= 65 )
			goto tr995;
	} else
		goto tr995;
	goto st816;
st816:
	if ( ++( p) == ( pe) )
		goto _test_eof816;
case 816:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr996;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
tr996:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st817;
st817:
	if ( ++( p) == ( pe) )
		goto _test_eof817;
case 817:
#line 19758 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st818;
	goto tr257;
st818:
	if ( ++( p) == ( pe) )
		goto _test_eof818;
case 818:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 35: goto tr999;
		case 47: goto tr999;
		case 72: goto tr1000;
		case 104: goto tr1000;
	}
	goto tr998;
tr998:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st819;
st819:
	if ( ++( p) == ( pe) )
		goto _test_eof819;
case 819:
#line 19784 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 41: goto tr1002;
	}
	goto st819;
tr1279:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st820;
tr999:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st820;
st820:
	if ( ++( p) == ( pe) )
		goto _test_eof820;
case 820:
#line 19806 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto st819;
		case 41: goto tr1004;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st819;
	goto st820;
tr1000:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st821;
st821:
	if ( ++( p) == ( pe) )
		goto _test_eof821;
case 821:
#line 19827 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 41: goto tr1002;
		case 84: goto st822;
		case 116: goto st822;
	}
	goto st819;
st822:
	if ( ++( p) == ( pe) )
		goto _test_eof822;
case 822:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 41: goto tr1002;
		case 84: goto st823;
		case 116: goto st823;
	}
	goto st819;
st823:
	if ( ++( p) == ( pe) )
		goto _test_eof823;
case 823:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 41: goto tr1002;
		case 80: goto st824;
		case 112: goto st824;
	}
	goto st819;
st824:
	if ( ++( p) == ( pe) )
		goto _test_eof824;
case 824:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 41: goto tr1002;
		case 58: goto st825;
		case 83: goto st828;
		case 115: goto st828;
	}
	goto st819;
st825:
	if ( ++( p) == ( pe) )
		goto _test_eof825;
case 825:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 41: goto tr1002;
		case 47: goto st826;
	}
	goto st819;
st826:
	if ( ++( p) == ( pe) )
		goto _test_eof826;
case 826:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 41: goto tr1002;
		case 47: goto st827;
	}
	goto st819;
st827:
	if ( ++( p) == ( pe) )
		goto _test_eof827;
case 827:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto st819;
		case 41: goto tr1012;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st819;
	goto st820;
tr1012:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 402 "ext/dtext/dtext.cpp.rl"
	{( act) = 54;}
	goto st1912;
tr1366:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 398 "ext/dtext/dtext.cpp.rl"
	{( act) = 53;}
	goto st1912;
st1912:
	if ( ++( p) == ( pe) )
		goto _test_eof1912;
case 1912:
#line 19933 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 32: goto tr250;
		case 41: goto tr986;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st806;
st828:
	if ( ++( p) == ( pe) )
		goto _test_eof828;
case 828:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
		case 41: goto tr1002;
		case 58: goto st825;
	}
	goto st819;
tr995:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st829;
st829:
	if ( ++( p) == ( pe) )
		goto _test_eof829;
case 829:
#line 19962 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 45: goto st829;
		case 93: goto tr1014;
		case 95: goto st829;
	}
	if ( (*( p)) < 48 ) {
		if ( 9 <= (*( p)) && (*( p)) <= 12 )
			goto st803;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st829;
		} else if ( (*( p)) >= 65 )
			goto st829;
	} else
		goto st829;
	goto st816;
tr1014:
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
	goto st1913;
tr1036:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 484 "ext/dtext/dtext.cpp.rl"
	{( act) = 77;}
	goto st1913;
tr1063:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 439 "ext/dtext/dtext.cpp.rl"
	{( act) = 71;}
	goto st1913;
tr1064:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 437 "ext/dtext/dtext.cpp.rl"
	{( act) = 69;}
	goto st1913;
st1913:
	if ( ++( p) == ( pe) )
		goto _test_eof1913;
case 1913:
#line 20030 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st818;
	goto tr250;
tr2441:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st830;
st830:
	if ( ++( p) == ( pe) )
		goto _test_eof830;
case 830:
#line 20044 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 66: goto st831;
		case 67: goto st833;
		case 69: goto st843;
		case 72: goto st849;
		case 73: goto st850;
		case 78: goto st851;
		case 81: goto st857;
		case 83: goto st862;
		case 84: goto st873;
		case 85: goto st884;
		case 93: goto tr996;
		case 98: goto st831;
		case 99: goto st833;
		case 101: goto st843;
		case 104: goto st849;
		case 105: goto st850;
		case 110: goto st851;
		case 113: goto st857;
		case 115: goto st862;
		case 116: goto st873;
		case 117: goto st884;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st831:
	if ( ++( p) == ( pe) )
		goto _test_eof831;
case 831:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 82: goto st832;
		case 93: goto tr1026;
		case 114: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st832:
	if ( ++( p) == ( pe) )
		goto _test_eof832;
case 832:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr257;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st833:
	if ( ++( p) == ( pe) )
		goto _test_eof833;
case 833:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 79: goto st834;
		case 93: goto tr996;
		case 111: goto st834;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st834:
	if ( ++( p) == ( pe) )
		goto _test_eof834;
case 834:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 68: goto st835;
		case 76: goto st836;
		case 93: goto tr996;
		case 100: goto st835;
		case 108: goto st836;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st835:
	if ( ++( p) == ( pe) )
		goto _test_eof835;
case 835:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 69: goto st832;
		case 93: goto tr996;
		case 101: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st836:
	if ( ++( p) == ( pe) )
		goto _test_eof836;
case 836:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 71: goto st837;
		case 79: goto st841;
		case 93: goto tr257;
		case 103: goto st837;
		case 111: goto st841;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st837:
	if ( ++( p) == ( pe) )
		goto _test_eof837;
case 837:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 82: goto st838;
		case 93: goto tr996;
		case 114: goto st838;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st838:
	if ( ++( p) == ( pe) )
		goto _test_eof838;
case 838:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 79: goto st839;
		case 93: goto tr996;
		case 111: goto st839;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st839:
	if ( ++( p) == ( pe) )
		goto _test_eof839;
case 839:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 85: goto st840;
		case 93: goto tr996;
		case 117: goto st840;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st840:
	if ( ++( p) == ( pe) )
		goto _test_eof840;
case 840:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 80: goto st832;
		case 93: goto tr996;
		case 112: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st841:
	if ( ++( p) == ( pe) )
		goto _test_eof841;
case 841:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 82: goto st842;
		case 93: goto tr996;
		case 114: goto st842;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st842:
	if ( ++( p) == ( pe) )
		goto _test_eof842;
case 842:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1036;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st843:
	if ( ++( p) == ( pe) )
		goto _test_eof843;
case 843:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 88: goto st844;
		case 93: goto tr996;
		case 120: goto st844;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st844:
	if ( ++( p) == ( pe) )
		goto _test_eof844;
case 844:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 80: goto st845;
		case 93: goto tr996;
		case 112: goto st845;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st845:
	if ( ++( p) == ( pe) )
		goto _test_eof845;
case 845:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 65: goto st846;
		case 93: goto tr996;
		case 97: goto st846;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st846:
	if ( ++( p) == ( pe) )
		goto _test_eof846;
case 846:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 78: goto st847;
		case 93: goto tr996;
		case 110: goto st847;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st847:
	if ( ++( p) == ( pe) )
		goto _test_eof847;
case 847:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 68: goto st848;
		case 93: goto tr996;
		case 100: goto st848;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st848:
	if ( ++( p) == ( pe) )
		goto _test_eof848;
case 848:
	_widec = (*( p));
	if ( 93 <= (*( p)) && (*( p)) <= 93 ) {
		_widec = (short)(2688 + ((*( p)) - -128));
		if ( 
#line 91 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(BLOCK_EXPAND)  ) _widec += 256;
	}
	switch( _widec ) {
		case 9: goto st803;
		case 32: goto st803;
		case 3165: goto st1710;
	}
	if ( _widec < 11 ) {
		if ( _widec > -1 ) {
			if ( 1 <= _widec && _widec <= 8 )
				goto st816;
		} else
			goto st816;
	} else if ( _widec > 12 ) {
		if ( _widec > 92 ) {
			if ( 94 <= _widec )
				goto st816;
		} else if ( _widec >= 14 )
			goto st816;
	} else
		goto st803;
	goto tr257;
st849:
	if ( ++( p) == ( pe) )
		goto _test_eof849;
case 849:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 82: goto st832;
		case 93: goto tr996;
		case 114: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st850:
	if ( ++( p) == ( pe) )
		goto _test_eof850;
case 850:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1042;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st851:
	if ( ++( p) == ( pe) )
		goto _test_eof851;
case 851:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 79: goto st852;
		case 93: goto tr996;
		case 111: goto st852;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st852:
	if ( ++( p) == ( pe) )
		goto _test_eof852;
case 852:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 68: goto st853;
		case 93: goto tr996;
		case 100: goto st853;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st853:
	if ( ++( p) == ( pe) )
		goto _test_eof853;
case 853:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 84: goto st854;
		case 93: goto tr996;
		case 116: goto st854;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st854:
	if ( ++( p) == ( pe) )
		goto _test_eof854;
case 854:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 69: goto st855;
		case 93: goto tr996;
		case 101: goto st855;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st855:
	if ( ++( p) == ( pe) )
		goto _test_eof855;
case 855:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 88: goto st856;
		case 93: goto tr996;
		case 120: goto st856;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st856:
	if ( ++( p) == ( pe) )
		goto _test_eof856;
case 856:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 84: goto st832;
		case 93: goto tr996;
		case 116: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st857:
	if ( ++( p) == ( pe) )
		goto _test_eof857;
case 857:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 85: goto st858;
		case 93: goto tr996;
		case 117: goto st858;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st858:
	if ( ++( p) == ( pe) )
		goto _test_eof858;
case 858:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 79: goto st859;
		case 93: goto tr996;
		case 111: goto st859;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st859:
	if ( ++( p) == ( pe) )
		goto _test_eof859;
case 859:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 84: goto st860;
		case 93: goto tr996;
		case 116: goto st860;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st860:
	if ( ++( p) == ( pe) )
		goto _test_eof860;
case 860:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 69: goto st861;
		case 93: goto tr996;
		case 101: goto st861;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
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
		case 9: goto st803;
		case 32: goto st803;
		case 2653: goto st1709;
	}
	if ( _widec < 11 ) {
		if ( _widec > -1 ) {
			if ( 1 <= _widec && _widec <= 8 )
				goto st816;
		} else
			goto st816;
	} else if ( _widec > 12 ) {
		if ( _widec > 92 ) {
			if ( 94 <= _widec )
				goto st816;
		} else if ( _widec >= 14 )
			goto st816;
	} else
		goto st803;
	goto tr257;
st862:
	if ( ++( p) == ( pe) )
		goto _test_eof862;
case 862:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 80: goto st863;
		case 85: goto st870;
		case 93: goto tr1054;
		case 112: goto st863;
		case 117: goto st870;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st863:
	if ( ++( p) == ( pe) )
		goto _test_eof863;
case 863:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 79: goto st864;
		case 93: goto tr996;
		case 111: goto st864;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st864:
	if ( ++( p) == ( pe) )
		goto _test_eof864;
case 864:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 73: goto st865;
		case 93: goto tr996;
		case 105: goto st865;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st865:
	if ( ++( p) == ( pe) )
		goto _test_eof865;
case 865:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 76: goto st866;
		case 93: goto tr996;
		case 108: goto st866;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st866:
	if ( ++( p) == ( pe) )
		goto _test_eof866;
case 866:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 69: goto st867;
		case 93: goto tr996;
		case 101: goto st867;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st867:
	if ( ++( p) == ( pe) )
		goto _test_eof867;
case 867:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 82: goto st868;
		case 93: goto tr996;
		case 114: goto st868;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st868:
	if ( ++( p) == ( pe) )
		goto _test_eof868;
case 868:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 83: goto st869;
		case 93: goto tr367;
		case 115: goto st869;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st869:
	if ( ++( p) == ( pe) )
		goto _test_eof869;
case 869:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr367;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st870:
	if ( ++( p) == ( pe) )
		goto _test_eof870;
case 870:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 66: goto st871;
		case 80: goto st872;
		case 93: goto tr996;
		case 98: goto st871;
		case 112: goto st872;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st871:
	if ( ++( p) == ( pe) )
		goto _test_eof871;
case 871:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1063;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st872:
	if ( ++( p) == ( pe) )
		goto _test_eof872;
case 872:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1064;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st873:
	if ( ++( p) == ( pe) )
		goto _test_eof873;
case 873:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 65: goto st874;
		case 66: goto st876;
		case 68: goto st879;
		case 72: goto st880;
		case 78: goto st883;
		case 82: goto st832;
		case 93: goto tr996;
		case 97: goto st874;
		case 98: goto st876;
		case 100: goto st879;
		case 104: goto st880;
		case 110: goto st883;
		case 114: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st874:
	if ( ++( p) == ( pe) )
		goto _test_eof874;
case 874:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 66: goto st875;
		case 93: goto tr996;
		case 98: goto st875;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st875:
	if ( ++( p) == ( pe) )
		goto _test_eof875;
case 875:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 76: goto st835;
		case 93: goto tr996;
		case 108: goto st835;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st876:
	if ( ++( p) == ( pe) )
		goto _test_eof876;
case 876:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 79: goto st877;
		case 93: goto tr996;
		case 111: goto st877;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st877:
	if ( ++( p) == ( pe) )
		goto _test_eof877;
case 877:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 68: goto st878;
		case 93: goto tr996;
		case 100: goto st878;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st878:
	if ( ++( p) == ( pe) )
		goto _test_eof878;
case 878:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 89: goto st832;
		case 93: goto tr996;
		case 121: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st879:
	if ( ++( p) == ( pe) )
		goto _test_eof879;
case 879:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr294;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st880:
	if ( ++( p) == ( pe) )
		goto _test_eof880;
case 880:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 69: goto st881;
		case 93: goto tr295;
		case 101: goto st881;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st881:
	if ( ++( p) == ( pe) )
		goto _test_eof881;
case 881:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 65: goto st882;
		case 93: goto tr996;
		case 97: goto st882;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st882:
	if ( ++( p) == ( pe) )
		goto _test_eof882;
case 882:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 68: goto st832;
		case 93: goto tr996;
		case 100: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st883:
	if ( ++( p) == ( pe) )
		goto _test_eof883;
case 883:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr346;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st884:
	if ( ++( p) == ( pe) )
		goto _test_eof884;
case 884:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 82: goto st885;
		case 93: goto tr1076;
		case 114: goto st885;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
st885:
	if ( ++( p) == ( pe) )
		goto _test_eof885;
case 885:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 76: goto st832;
		case 93: goto tr996;
		case 108: goto st832;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st816;
tr2442:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st886;
st886:
	if ( ++( p) == ( pe) )
		goto _test_eof886;
case 886:
#line 20995 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st887;
		case 93: goto tr1078;
		case 114: goto st887;
	}
	goto st803;
st887:
	if ( ++( p) == ( pe) )
		goto _test_eof887;
case 887:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1079;
	}
	goto st803;
tr2443:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st888;
st888:
	if ( ++( p) == ( pe) )
		goto _test_eof888;
case 888:
#line 21024 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st889;
		case 93: goto tr981;
		case 111: goto st889;
	}
	goto st803;
st889:
	if ( ++( p) == ( pe) )
		goto _test_eof889;
case 889:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 68: goto st890;
		case 76: goto st897;
		case 93: goto tr981;
		case 100: goto st890;
		case 108: goto st897;
	}
	goto st803;
st890:
	if ( ++( p) == ( pe) )
		goto _test_eof890;
case 890:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st891;
		case 93: goto tr981;
		case 101: goto st891;
	}
	goto st803;
st891:
	if ( ++( p) == ( pe) )
		goto _test_eof891;
case 891:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st892;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st892;
		case 61: goto st893;
		case 93: goto tr1086;
	}
	goto st803;
st892:
	if ( ++( p) == ( pe) )
		goto _test_eof892;
case 892:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st892;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st892;
		case 61: goto st893;
		case 93: goto tr981;
	}
	goto st803;
st893:
	if ( ++( p) == ( pe) )
		goto _test_eof893;
case 893:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st893;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st893;
		case 93: goto tr981;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1087;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1087;
	} else
		goto tr1087;
	goto st803;
tr1087:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st894;
st894:
	if ( ++( p) == ( pe) )
		goto _test_eof894;
case 894:
#line 21119 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1089;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st894;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st894;
	} else
		goto st894;
	goto st803;
tr1089:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 503 "ext/dtext/dtext.cpp.rl"
	{( act) = 80;}
	goto st1914;
st1914:
	if ( ++( p) == ( pe) )
		goto _test_eof1914;
case 1914:
#line 21149 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1091;
		case 9: goto st895;
		case 10: goto tr1091;
		case 32: goto st895;
		case 40: goto st805;
	}
	goto tr2453;
st895:
	if ( ++( p) == ( pe) )
		goto _test_eof895;
case 895:
	switch( (*( p)) ) {
		case 0: goto tr1091;
		case 9: goto st895;
		case 10: goto tr1091;
		case 32: goto st895;
	}
	goto tr1090;
tr1086:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1915;
st1915:
	if ( ++( p) == ( pe) )
		goto _test_eof1915;
case 1915:
#line 21177 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1094;
		case 9: goto st896;
		case 10: goto tr1094;
		case 32: goto st896;
	}
	goto tr2454;
st896:
	if ( ++( p) == ( pe) )
		goto _test_eof896;
case 896:
	switch( (*( p)) ) {
		case 0: goto tr1094;
		case 9: goto st896;
		case 10: goto tr1094;
		case 32: goto st896;
	}
	goto tr1093;
st897:
	if ( ++( p) == ( pe) )
		goto _test_eof897;
case 897:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 71: goto st898;
		case 79: goto st903;
		case 93: goto tr257;
		case 103: goto st898;
		case 111: goto st903;
	}
	goto st803;
st898:
	if ( ++( p) == ( pe) )
		goto _test_eof898;
case 898:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st899;
		case 93: goto tr981;
		case 114: goto st899;
	}
	goto st803;
st899:
	if ( ++( p) == ( pe) )
		goto _test_eof899;
case 899:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st900;
		case 93: goto tr981;
		case 111: goto st900;
	}
	goto st803;
st900:
	if ( ++( p) == ( pe) )
		goto _test_eof900;
case 900:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 85: goto st901;
		case 93: goto tr981;
		case 117: goto st901;
	}
	goto st803;
st901:
	if ( ++( p) == ( pe) )
		goto _test_eof901;
case 901:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st902;
		case 93: goto tr981;
		case 112: goto st902;
	}
	goto st803;
st902:
	if ( ++( p) == ( pe) )
		goto _test_eof902;
case 902:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr257;
	}
	goto st803;
st903:
	if ( ++( p) == ( pe) )
		goto _test_eof903;
case 903:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st904;
		case 93: goto tr981;
		case 114: goto st904;
	}
	goto st803;
st904:
	if ( ++( p) == ( pe) )
		goto _test_eof904;
case 904:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 61: goto st905;
		case 93: goto tr981;
	}
	goto st803;
st905:
	if ( ++( p) == ( pe) )
		goto _test_eof905;
case 905:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 35: goto tr1104;
		case 65: goto tr1105;
		case 67: goto tr1106;
		case 69: goto tr1107;
		case 71: goto tr1108;
		case 73: goto tr1109;
		case 76: goto tr1110;
		case 77: goto tr1111;
		case 79: goto tr1112;
		case 81: goto tr1113;
		case 83: goto tr1114;
		case 86: goto tr1115;
		case 93: goto tr981;
		case 97: goto tr1116;
		case 99: goto tr1118;
		case 101: goto tr1119;
		case 103: goto tr1120;
		case 105: goto tr1121;
		case 108: goto tr1122;
		case 109: goto tr1123;
		case 111: goto tr1124;
		case 113: goto tr1125;
		case 115: goto tr1126;
		case 118: goto tr1127;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto tr1117;
	goto st803;
tr1104:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st906;
st906:
	if ( ++( p) == ( pe) )
		goto _test_eof906;
case 906:
#line 21343 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st907;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st907;
	} else
		goto st907;
	goto st803;
st907:
	if ( ++( p) == ( pe) )
		goto _test_eof907;
case 907:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st908;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st908;
	} else
		goto st908;
	goto st803;
st908:
	if ( ++( p) == ( pe) )
		goto _test_eof908;
case 908:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st909;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st909;
	} else
		goto st909;
	goto st803;
st909:
	if ( ++( p) == ( pe) )
		goto _test_eof909;
case 909:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1132;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st910;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st910;
	} else
		goto st910;
	goto st803;
st910:
	if ( ++( p) == ( pe) )
		goto _test_eof910;
case 910:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1132;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st911;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st911;
	} else
		goto st911;
	goto st803;
st911:
	if ( ++( p) == ( pe) )
		goto _test_eof911;
case 911:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1132;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st912;
	} else if ( (*( p)) > 70 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 102 )
			goto st912;
	} else
		goto st912;
	goto st803;
st912:
	if ( ++( p) == ( pe) )
		goto _test_eof912;
case 912:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1132;
	}
	goto st803;
tr1132:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 471 "ext/dtext/dtext.cpp.rl"
	{( act) = 76;}
	goto st1916;
tr1138:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 463 "ext/dtext/dtext.cpp.rl"
	{( act) = 75;}
	goto st1916;
tr1315:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 438 "ext/dtext/dtext.cpp.rl"
	{( act) = 70;}
	goto st1916;
tr1316:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
#line 436 "ext/dtext/dtext.cpp.rl"
	{( act) = 68;}
	goto st1916;
st1916:
	if ( ++( p) == ( pe) )
		goto _test_eof1916;
case 1916:
#line 21505 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 40 )
		goto st805;
	goto tr250;
tr1105:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st913;
st913:
	if ( ++( p) == ( pe) )
		goto _test_eof913;
case 913:
#line 21517 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st914;
		case 93: goto tr981;
		case 114: goto st914;
	}
	goto st803;
st914:
	if ( ++( p) == ( pe) )
		goto _test_eof914;
case 914:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st915;
		case 93: goto tr981;
		case 116: goto st915;
	}
	goto st803;
st915:
	if ( ++( p) == ( pe) )
		goto _test_eof915;
case 915:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st916;
		case 93: goto tr1138;
		case 105: goto st916;
	}
	goto st803;
st916:
	if ( ++( p) == ( pe) )
		goto _test_eof916;
case 916:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 83: goto st917;
		case 93: goto tr981;
		case 115: goto st917;
	}
	goto st803;
st917:
	if ( ++( p) == ( pe) )
		goto _test_eof917;
case 917:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st918;
		case 93: goto tr981;
		case 116: goto st918;
	}
	goto st803;
st918:
	if ( ++( p) == ( pe) )
		goto _test_eof918;
case 918:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1138;
	}
	goto st803;
tr1106:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st919;
st919:
	if ( ++( p) == ( pe) )
		goto _test_eof919;
case 919:
#line 21598 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 72: goto st920;
		case 79: goto st927;
		case 93: goto tr981;
		case 104: goto st920;
		case 111: goto st927;
	}
	goto st803;
st920:
	if ( ++( p) == ( pe) )
		goto _test_eof920;
case 920:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st921;
		case 93: goto tr1138;
		case 97: goto st921;
	}
	goto st803;
st921:
	if ( ++( p) == ( pe) )
		goto _test_eof921;
case 921:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st922;
		case 93: goto tr981;
		case 114: goto st922;
	}
	goto st803;
st922:
	if ( ++( p) == ( pe) )
		goto _test_eof922;
case 922:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st923;
		case 93: goto tr1138;
		case 97: goto st923;
	}
	goto st803;
st923:
	if ( ++( p) == ( pe) )
		goto _test_eof923;
case 923:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st924;
		case 93: goto tr981;
		case 99: goto st924;
	}
	goto st803;
st924:
	if ( ++( p) == ( pe) )
		goto _test_eof924;
case 924:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st925;
		case 93: goto tr981;
		case 116: goto st925;
	}
	goto st803;
st925:
	if ( ++( p) == ( pe) )
		goto _test_eof925;
case 925:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st926;
		case 93: goto tr981;
		case 101: goto st926;
	}
	goto st803;
st926:
	if ( ++( p) == ( pe) )
		goto _test_eof926;
case 926:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st918;
		case 93: goto tr981;
		case 114: goto st918;
	}
	goto st803;
st927:
	if ( ++( p) == ( pe) )
		goto _test_eof927;
case 927:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st928;
		case 93: goto tr1138;
		case 112: goto st928;
	}
	goto st803;
st928:
	if ( ++( p) == ( pe) )
		goto _test_eof928;
case 928:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 89: goto st929;
		case 93: goto tr981;
		case 121: goto st929;
	}
	goto st803;
st929:
	if ( ++( p) == ( pe) )
		goto _test_eof929;
case 929:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st930;
		case 93: goto tr1138;
		case 114: goto st930;
	}
	goto st803;
st930:
	if ( ++( p) == ( pe) )
		goto _test_eof930;
case 930:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st931;
		case 93: goto tr981;
		case 105: goto st931;
	}
	goto st803;
st931:
	if ( ++( p) == ( pe) )
		goto _test_eof931;
case 931:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 71: goto st932;
		case 93: goto tr981;
		case 103: goto st932;
	}
	goto st803;
st932:
	if ( ++( p) == ( pe) )
		goto _test_eof932;
case 932:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 72: goto st917;
		case 93: goto tr981;
		case 104: goto st917;
	}
	goto st803;
tr1107:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st933;
st933:
	if ( ++( p) == ( pe) )
		goto _test_eof933;
case 933:
#line 21787 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 88: goto st934;
		case 93: goto tr1138;
		case 120: goto st934;
	}
	goto st803;
st934:
	if ( ++( p) == ( pe) )
		goto _test_eof934;
case 934:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st935;
		case 93: goto tr981;
		case 112: goto st935;
	}
	goto st803;
st935:
	if ( ++( p) == ( pe) )
		goto _test_eof935;
case 935:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st936;
		case 93: goto tr981;
		case 108: goto st936;
	}
	goto st803;
st936:
	if ( ++( p) == ( pe) )
		goto _test_eof936;
case 936:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st937;
		case 93: goto tr981;
		case 105: goto st937;
	}
	goto st803;
st937:
	if ( ++( p) == ( pe) )
		goto _test_eof937;
case 937:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st938;
		case 93: goto tr981;
		case 99: goto st938;
	}
	goto st803;
st938:
	if ( ++( p) == ( pe) )
		goto _test_eof938;
case 938:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st917;
		case 93: goto tr981;
		case 105: goto st917;
	}
	goto st803;
tr1108:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st939;
st939:
	if ( ++( p) == ( pe) )
		goto _test_eof939;
case 939:
#line 21870 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st940;
		case 93: goto tr981;
		case 101: goto st940;
	}
	goto st803;
st940:
	if ( ++( p) == ( pe) )
		goto _test_eof940;
case 940:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 78: goto st941;
		case 93: goto tr981;
		case 110: goto st941;
	}
	goto st803;
st941:
	if ( ++( p) == ( pe) )
		goto _test_eof941;
case 941:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st942;
		case 93: goto tr1138;
		case 101: goto st942;
	}
	goto st803;
st942:
	if ( ++( p) == ( pe) )
		goto _test_eof942;
case 942:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st943;
		case 93: goto tr981;
		case 114: goto st943;
	}
	goto st803;
st943:
	if ( ++( p) == ( pe) )
		goto _test_eof943;
case 943:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st944;
		case 93: goto tr981;
		case 97: goto st944;
	}
	goto st803;
st944:
	if ( ++( p) == ( pe) )
		goto _test_eof944;
case 944:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st918;
		case 93: goto tr981;
		case 108: goto st918;
	}
	goto st803;
tr1109:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st945;
st945:
	if ( ++( p) == ( pe) )
		goto _test_eof945;
case 945:
#line 21953 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 78: goto st946;
		case 93: goto tr981;
		case 110: goto st946;
	}
	goto st803;
st946:
	if ( ++( p) == ( pe) )
		goto _test_eof946;
case 946:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 86: goto st947;
		case 93: goto tr981;
		case 118: goto st947;
	}
	goto st803;
st947:
	if ( ++( p) == ( pe) )
		goto _test_eof947;
case 947:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st948;
		case 93: goto tr1138;
		case 97: goto st948;
	}
	goto st803;
st948:
	if ( ++( p) == ( pe) )
		goto _test_eof948;
case 948:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st949;
		case 93: goto tr981;
		case 108: goto st949;
	}
	goto st803;
st949:
	if ( ++( p) == ( pe) )
		goto _test_eof949;
case 949:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st950;
		case 93: goto tr981;
		case 105: goto st950;
	}
	goto st803;
st950:
	if ( ++( p) == ( pe) )
		goto _test_eof950;
case 950:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 68: goto st918;
		case 93: goto tr981;
		case 100: goto st918;
	}
	goto st803;
tr1110:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st951;
st951:
	if ( ++( p) == ( pe) )
		goto _test_eof951;
case 951:
#line 22036 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st952;
		case 93: goto tr981;
		case 111: goto st952;
	}
	goto st803;
st952:
	if ( ++( p) == ( pe) )
		goto _test_eof952;
case 952:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st953;
		case 93: goto tr981;
		case 114: goto st953;
	}
	goto st803;
st953:
	if ( ++( p) == ( pe) )
		goto _test_eof953;
case 953:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st918;
		case 93: goto tr1138;
		case 101: goto st918;
	}
	goto st803;
tr1111:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st954;
st954:
	if ( ++( p) == ( pe) )
		goto _test_eof954;
case 954:
#line 22080 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st955;
		case 93: goto tr981;
		case 101: goto st955;
	}
	goto st803;
st955:
	if ( ++( p) == ( pe) )
		goto _test_eof955;
case 955:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st956;
		case 93: goto tr981;
		case 116: goto st956;
	}
	goto st803;
st956:
	if ( ++( p) == ( pe) )
		goto _test_eof956;
case 956:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st918;
		case 93: goto tr981;
		case 97: goto st918;
	}
	goto st803;
tr1112:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st957;
st957:
	if ( ++( p) == ( pe) )
		goto _test_eof957;
case 957:
#line 22124 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st918;
		case 93: goto tr981;
		case 99: goto st918;
	}
	goto st803;
tr1113:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st958;
st958:
	if ( ++( p) == ( pe) )
		goto _test_eof958;
case 958:
#line 22142 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 85: goto st959;
		case 93: goto tr1138;
		case 117: goto st959;
	}
	goto st803;
st959:
	if ( ++( p) == ( pe) )
		goto _test_eof959;
case 959:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st960;
		case 93: goto tr981;
		case 101: goto st960;
	}
	goto st803;
st960:
	if ( ++( p) == ( pe) )
		goto _test_eof960;
case 960:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 83: goto st961;
		case 93: goto tr981;
		case 115: goto st961;
	}
	goto st803;
st961:
	if ( ++( p) == ( pe) )
		goto _test_eof961;
case 961:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st962;
		case 93: goto tr981;
		case 116: goto st962;
	}
	goto st803;
st962:
	if ( ++( p) == ( pe) )
		goto _test_eof962;
case 962:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st963;
		case 93: goto tr981;
		case 105: goto st963;
	}
	goto st803;
st963:
	if ( ++( p) == ( pe) )
		goto _test_eof963;
case 963:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st964;
		case 93: goto tr981;
		case 111: goto st964;
	}
	goto st803;
st964:
	if ( ++( p) == ( pe) )
		goto _test_eof964;
case 964:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 78: goto st965;
		case 93: goto tr981;
		case 110: goto st965;
	}
	goto st803;
st965:
	if ( ++( p) == ( pe) )
		goto _test_eof965;
case 965:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st966;
		case 93: goto tr981;
		case 97: goto st966;
	}
	goto st803;
st966:
	if ( ++( p) == ( pe) )
		goto _test_eof966;
case 966:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 66: goto st967;
		case 93: goto tr981;
		case 98: goto st967;
	}
	goto st803;
st967:
	if ( ++( p) == ( pe) )
		goto _test_eof967;
case 967:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st968;
		case 93: goto tr981;
		case 108: goto st968;
	}
	goto st803;
st968:
	if ( ++( p) == ( pe) )
		goto _test_eof968;
case 968:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st918;
		case 93: goto tr981;
		case 101: goto st918;
	}
	goto st803;
tr1114:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st969;
st969:
	if ( ++( p) == ( pe) )
		goto _test_eof969;
case 969:
#line 22290 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st970;
		case 80: goto st971;
		case 93: goto tr1138;
		case 97: goto st970;
		case 112: goto st971;
	}
	goto st803;
st970:
	if ( ++( p) == ( pe) )
		goto _test_eof970;
case 970:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 70: goto st968;
		case 93: goto tr981;
		case 102: goto st968;
	}
	goto st803;
st971:
	if ( ++( p) == ( pe) )
		goto _test_eof971;
case 971:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st972;
		case 93: goto tr981;
		case 101: goto st972;
	}
	goto st803;
st972:
	if ( ++( p) == ( pe) )
		goto _test_eof972;
case 972:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st973;
		case 93: goto tr981;
		case 99: goto st973;
	}
	goto st803;
st973:
	if ( ++( p) == ( pe) )
		goto _test_eof973;
case 973:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st974;
		case 93: goto tr1138;
		case 105: goto st974;
	}
	goto st803;
st974:
	if ( ++( p) == ( pe) )
		goto _test_eof974;
case 974:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st975;
		case 93: goto tr981;
		case 101: goto st975;
	}
	goto st803;
st975:
	if ( ++( p) == ( pe) )
		goto _test_eof975;
case 975:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 83: goto st918;
		case 93: goto tr981;
		case 115: goto st918;
	}
	goto st803;
tr1115:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st976;
st976:
	if ( ++( p) == ( pe) )
		goto _test_eof976;
case 976:
#line 22388 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st977;
		case 93: goto tr981;
		case 111: goto st977;
	}
	goto st803;
st977:
	if ( ++( p) == ( pe) )
		goto _test_eof977;
case 977:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st978;
		case 93: goto tr981;
		case 105: goto st978;
	}
	goto st803;
st978:
	if ( ++( p) == ( pe) )
		goto _test_eof978;
case 978:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st979;
		case 93: goto tr981;
		case 99: goto st979;
	}
	goto st803;
st979:
	if ( ++( p) == ( pe) )
		goto _test_eof979;
case 979:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st980;
		case 93: goto tr981;
		case 101: goto st980;
	}
	goto st803;
st980:
	if ( ++( p) == ( pe) )
		goto _test_eof980;
case 980:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 45: goto st981;
		case 93: goto tr981;
	}
	goto st803;
st981:
	if ( ++( p) == ( pe) )
		goto _test_eof981;
case 981:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st982;
		case 93: goto tr981;
		case 97: goto st982;
	}
	goto st803;
st982:
	if ( ++( p) == ( pe) )
		goto _test_eof982;
case 982:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st983;
		case 93: goto tr981;
		case 99: goto st983;
	}
	goto st803;
st983:
	if ( ++( p) == ( pe) )
		goto _test_eof983;
case 983:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st984;
		case 93: goto tr981;
		case 116: goto st984;
	}
	goto st803;
st984:
	if ( ++( p) == ( pe) )
		goto _test_eof984;
case 984:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st926;
		case 93: goto tr981;
		case 111: goto st926;
	}
	goto st803;
tr1116:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st985;
st985:
	if ( ++( p) == ( pe) )
		goto _test_eof985;
case 985:
#line 22509 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st914;
		case 93: goto tr1132;
		case 114: goto st987;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1117:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st986;
st986:
	if ( ++( p) == ( pe) )
		goto _test_eof986;
case 986:
#line 22529 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1132;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st987:
	if ( ++( p) == ( pe) )
		goto _test_eof987;
case 987:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st915;
		case 93: goto tr1132;
		case 116: goto st988;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st988:
	if ( ++( p) == ( pe) )
		goto _test_eof988;
case 988:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st916;
		case 93: goto tr1138;
		case 105: goto st989;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st989:
	if ( ++( p) == ( pe) )
		goto _test_eof989;
case 989:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 83: goto st917;
		case 93: goto tr1132;
		case 115: goto st990;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st990:
	if ( ++( p) == ( pe) )
		goto _test_eof990;
case 990:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st918;
		case 93: goto tr1132;
		case 116: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st991:
	if ( ++( p) == ( pe) )
		goto _test_eof991;
case 991:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1138;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1118:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st992;
st992:
	if ( ++( p) == ( pe) )
		goto _test_eof992;
case 992:
#line 22620 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 72: goto st920;
		case 79: goto st927;
		case 93: goto tr1132;
		case 104: goto st993;
		case 111: goto st1000;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st993:
	if ( ++( p) == ( pe) )
		goto _test_eof993;
case 993:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st921;
		case 93: goto tr1138;
		case 97: goto st994;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st994:
	if ( ++( p) == ( pe) )
		goto _test_eof994;
case 994:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st922;
		case 93: goto tr1132;
		case 114: goto st995;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st995:
	if ( ++( p) == ( pe) )
		goto _test_eof995;
case 995:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st923;
		case 93: goto tr1138;
		case 97: goto st996;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st996:
	if ( ++( p) == ( pe) )
		goto _test_eof996;
case 996:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st924;
		case 93: goto tr1132;
		case 99: goto st997;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st997:
	if ( ++( p) == ( pe) )
		goto _test_eof997;
case 997:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st925;
		case 93: goto tr1132;
		case 116: goto st998;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st998:
	if ( ++( p) == ( pe) )
		goto _test_eof998;
case 998:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st926;
		case 93: goto tr1132;
		case 101: goto st999;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st999:
	if ( ++( p) == ( pe) )
		goto _test_eof999;
case 999:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st918;
		case 93: goto tr1132;
		case 114: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1000:
	if ( ++( p) == ( pe) )
		goto _test_eof1000;
case 1000:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st928;
		case 93: goto tr1138;
		case 112: goto st1001;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1001:
	if ( ++( p) == ( pe) )
		goto _test_eof1001;
case 1001:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 89: goto st929;
		case 93: goto tr1132;
		case 121: goto st1002;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1002:
	if ( ++( p) == ( pe) )
		goto _test_eof1002;
case 1002:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st930;
		case 93: goto tr1138;
		case 114: goto st1003;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1003:
	if ( ++( p) == ( pe) )
		goto _test_eof1003;
case 1003:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st931;
		case 93: goto tr1132;
		case 105: goto st1004;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1004:
	if ( ++( p) == ( pe) )
		goto _test_eof1004;
case 1004:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 71: goto st932;
		case 93: goto tr1132;
		case 103: goto st1005;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1005:
	if ( ++( p) == ( pe) )
		goto _test_eof1005;
case 1005:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 72: goto st917;
		case 93: goto tr1132;
		case 104: goto st990;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1119:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1006;
st1006:
	if ( ++( p) == ( pe) )
		goto _test_eof1006;
case 1006:
#line 22837 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 88: goto st934;
		case 93: goto tr1138;
		case 120: goto st1007;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1007:
	if ( ++( p) == ( pe) )
		goto _test_eof1007;
case 1007:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st935;
		case 93: goto tr1132;
		case 112: goto st1008;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1008:
	if ( ++( p) == ( pe) )
		goto _test_eof1008;
case 1008:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st936;
		case 93: goto tr1132;
		case 108: goto st1009;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1009:
	if ( ++( p) == ( pe) )
		goto _test_eof1009;
case 1009:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st937;
		case 93: goto tr1132;
		case 105: goto st1010;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1010:
	if ( ++( p) == ( pe) )
		goto _test_eof1010;
case 1010:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st938;
		case 93: goto tr1132;
		case 99: goto st1011;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1011:
	if ( ++( p) == ( pe) )
		goto _test_eof1011;
case 1011:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st917;
		case 93: goto tr1132;
		case 105: goto st990;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1120:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1012;
st1012:
	if ( ++( p) == ( pe) )
		goto _test_eof1012;
case 1012:
#line 22932 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st940;
		case 93: goto tr1132;
		case 101: goto st1013;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1013:
	if ( ++( p) == ( pe) )
		goto _test_eof1013;
case 1013:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 78: goto st941;
		case 93: goto tr1132;
		case 110: goto st1014;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1014:
	if ( ++( p) == ( pe) )
		goto _test_eof1014;
case 1014:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st942;
		case 93: goto tr1138;
		case 101: goto st1015;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1015:
	if ( ++( p) == ( pe) )
		goto _test_eof1015;
case 1015:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st943;
		case 93: goto tr1132;
		case 114: goto st1016;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1016:
	if ( ++( p) == ( pe) )
		goto _test_eof1016;
case 1016:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st944;
		case 93: goto tr1132;
		case 97: goto st1017;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1017:
	if ( ++( p) == ( pe) )
		goto _test_eof1017;
case 1017:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st918;
		case 93: goto tr1132;
		case 108: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1121:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1018;
st1018:
	if ( ++( p) == ( pe) )
		goto _test_eof1018;
case 1018:
#line 23027 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 78: goto st946;
		case 93: goto tr1132;
		case 110: goto st1019;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1019:
	if ( ++( p) == ( pe) )
		goto _test_eof1019;
case 1019:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 86: goto st947;
		case 93: goto tr1132;
		case 118: goto st1020;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1020:
	if ( ++( p) == ( pe) )
		goto _test_eof1020;
case 1020:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st948;
		case 93: goto tr1138;
		case 97: goto st1021;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1021:
	if ( ++( p) == ( pe) )
		goto _test_eof1021;
case 1021:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st949;
		case 93: goto tr1132;
		case 108: goto st1022;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1022:
	if ( ++( p) == ( pe) )
		goto _test_eof1022;
case 1022:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st950;
		case 93: goto tr1132;
		case 105: goto st1023;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1023:
	if ( ++( p) == ( pe) )
		goto _test_eof1023;
case 1023:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 68: goto st918;
		case 93: goto tr1132;
		case 100: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1122:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1024;
st1024:
	if ( ++( p) == ( pe) )
		goto _test_eof1024;
case 1024:
#line 23122 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st952;
		case 93: goto tr1132;
		case 111: goto st1025;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1025:
	if ( ++( p) == ( pe) )
		goto _test_eof1025;
case 1025:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st953;
		case 93: goto tr1132;
		case 114: goto st1026;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1026:
	if ( ++( p) == ( pe) )
		goto _test_eof1026;
case 1026:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st918;
		case 93: goto tr1138;
		case 101: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1123:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1027;
st1027:
	if ( ++( p) == ( pe) )
		goto _test_eof1027;
case 1027:
#line 23172 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st955;
		case 93: goto tr1132;
		case 101: goto st1028;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1028:
	if ( ++( p) == ( pe) )
		goto _test_eof1028;
case 1028:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st956;
		case 93: goto tr1132;
		case 116: goto st1029;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1029:
	if ( ++( p) == ( pe) )
		goto _test_eof1029;
case 1029:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st918;
		case 93: goto tr1132;
		case 97: goto st991;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1124:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1030;
st1030:
	if ( ++( p) == ( pe) )
		goto _test_eof1030;
case 1030:
#line 23222 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st918;
		case 93: goto tr1132;
		case 99: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1125:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1031;
st1031:
	if ( ++( p) == ( pe) )
		goto _test_eof1031;
case 1031:
#line 23242 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 85: goto st959;
		case 93: goto tr1138;
		case 117: goto st1032;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1032:
	if ( ++( p) == ( pe) )
		goto _test_eof1032;
case 1032:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st960;
		case 93: goto tr1132;
		case 101: goto st1033;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1033:
	if ( ++( p) == ( pe) )
		goto _test_eof1033;
case 1033:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 83: goto st961;
		case 93: goto tr1132;
		case 115: goto st1034;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1034:
	if ( ++( p) == ( pe) )
		goto _test_eof1034;
case 1034:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st962;
		case 93: goto tr1132;
		case 116: goto st1035;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1035:
	if ( ++( p) == ( pe) )
		goto _test_eof1035;
case 1035:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st963;
		case 93: goto tr1132;
		case 105: goto st1036;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1036:
	if ( ++( p) == ( pe) )
		goto _test_eof1036;
case 1036:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st964;
		case 93: goto tr1132;
		case 111: goto st1037;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1037:
	if ( ++( p) == ( pe) )
		goto _test_eof1037;
case 1037:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 78: goto st965;
		case 93: goto tr1132;
		case 110: goto st1038;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1038:
	if ( ++( p) == ( pe) )
		goto _test_eof1038;
case 1038:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st966;
		case 93: goto tr1132;
		case 97: goto st1039;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1039:
	if ( ++( p) == ( pe) )
		goto _test_eof1039;
case 1039:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 66: goto st967;
		case 93: goto tr1132;
		case 98: goto st1040;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1040:
	if ( ++( p) == ( pe) )
		goto _test_eof1040;
case 1040:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st968;
		case 93: goto tr1132;
		case 108: goto st1041;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1041:
	if ( ++( p) == ( pe) )
		goto _test_eof1041;
case 1041:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st918;
		case 93: goto tr1132;
		case 101: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1126:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1042;
st1042:
	if ( ++( p) == ( pe) )
		goto _test_eof1042;
case 1042:
#line 23412 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st970;
		case 80: goto st971;
		case 93: goto tr1138;
		case 97: goto st1043;
		case 112: goto st1044;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1043:
	if ( ++( p) == ( pe) )
		goto _test_eof1043;
case 1043:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 70: goto st968;
		case 93: goto tr1132;
		case 102: goto st1041;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1044:
	if ( ++( p) == ( pe) )
		goto _test_eof1044;
case 1044:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st972;
		case 93: goto tr1132;
		case 101: goto st1045;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1045:
	if ( ++( p) == ( pe) )
		goto _test_eof1045;
case 1045:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st973;
		case 93: goto tr1132;
		case 99: goto st1046;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1046:
	if ( ++( p) == ( pe) )
		goto _test_eof1046;
case 1046:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st974;
		case 93: goto tr1138;
		case 105: goto st1047;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1047:
	if ( ++( p) == ( pe) )
		goto _test_eof1047;
case 1047:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st975;
		case 93: goto tr1132;
		case 101: goto st1048;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1048:
	if ( ++( p) == ( pe) )
		goto _test_eof1048;
case 1048:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 83: goto st918;
		case 93: goto tr1132;
		case 115: goto st991;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr1127:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1049;
st1049:
	if ( ++( p) == ( pe) )
		goto _test_eof1049;
case 1049:
#line 23524 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st977;
		case 93: goto tr1132;
		case 97: goto st991;
		case 111: goto st1050;
	}
	if ( 98 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1050:
	if ( ++( p) == ( pe) )
		goto _test_eof1050;
case 1050:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st978;
		case 93: goto tr1132;
		case 105: goto st1051;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1051:
	if ( ++( p) == ( pe) )
		goto _test_eof1051;
case 1051:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 67: goto st979;
		case 93: goto tr1132;
		case 99: goto st1052;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1052:
	if ( ++( p) == ( pe) )
		goto _test_eof1052;
case 1052:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st980;
		case 93: goto tr1132;
		case 101: goto st1053;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
st1053:
	if ( ++( p) == ( pe) )
		goto _test_eof1053;
case 1053:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 45: goto st981;
		case 93: goto tr1132;
	}
	if ( 97 <= (*( p)) && (*( p)) <= 122 )
		goto st986;
	goto st803;
tr2444:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1054;
st1054:
	if ( ++( p) == ( pe) )
		goto _test_eof1054;
case 1054:
#line 23604 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 88: goto st1055;
		case 93: goto tr981;
		case 120: goto st1055;
	}
	goto st803;
st1055:
	if ( ++( p) == ( pe) )
		goto _test_eof1055;
case 1055:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st1056;
		case 93: goto tr981;
		case 112: goto st1056;
	}
	goto st803;
st1056:
	if ( ++( p) == ( pe) )
		goto _test_eof1056;
case 1056:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st1057;
		case 93: goto tr981;
		case 97: goto st1057;
	}
	goto st803;
st1057:
	if ( ++( p) == ( pe) )
		goto _test_eof1057;
case 1057:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 78: goto st1058;
		case 93: goto tr981;
		case 110: goto st1058;
	}
	goto st803;
st1058:
	if ( ++( p) == ( pe) )
		goto _test_eof1058;
case 1058:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 68: goto st902;
		case 93: goto tr981;
		case 100: goto st902;
	}
	goto st803;
tr2445:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1059;
st1059:
	if ( ++( p) == ( pe) )
		goto _test_eof1059;
case 1059:
#line 23676 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st902;
		case 84: goto st1060;
		case 93: goto tr981;
		case 114: goto st902;
		case 116: goto st1060;
	}
	goto st803;
st1060:
	if ( ++( p) == ( pe) )
		goto _test_eof1060;
case 1060:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1061;
		case 93: goto tr981;
		case 116: goto st1061;
	}
	goto st803;
st1061:
	if ( ++( p) == ( pe) )
		goto _test_eof1061;
case 1061:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st1062;
		case 93: goto tr981;
		case 112: goto st1062;
	}
	goto st803;
st1062:
	if ( ++( p) == ( pe) )
		goto _test_eof1062;
case 1062:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 58: goto st1063;
		case 83: goto st1084;
		case 93: goto tr981;
		case 115: goto st1084;
	}
	goto st803;
st1063:
	if ( ++( p) == ( pe) )
		goto _test_eof1063;
case 1063:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1064;
		case 93: goto tr981;
	}
	goto st803;
st1064:
	if ( ++( p) == ( pe) )
		goto _test_eof1064;
case 1064:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1065;
		case 93: goto tr981;
	}
	goto st803;
st1065:
	if ( ++( p) == ( pe) )
		goto _test_eof1065;
case 1065:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1267;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1066;
st1066:
	if ( ++( p) == ( pe) )
		goto _test_eof1066;
case 1066:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1268;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1066;
tr1268:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1067;
st1067:
	if ( ++( p) == ( pe) )
		goto _test_eof1067;
case 1067:
#line 23790 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 40: goto st818;
		case 93: goto tr1270;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1068:
	if ( ++( p) == ( pe) )
		goto _test_eof1068;
case 1068:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 32: goto tr250;
		case 93: goto tr1270;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st1068;
tr1270:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1069;
st1069:
	if ( ++( p) == ( pe) )
		goto _test_eof1069;
case 1069:
#line 23820 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 32: goto tr250;
		case 40: goto st1070;
		case 93: goto tr1270;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st1068;
st1070:
	if ( ++( p) == ( pe) )
		goto _test_eof1070;
case 1070:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 10: goto tr250;
		case 13: goto tr250;
	}
	goto tr998;
tr1267:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1071;
st1071:
	if ( ++( p) == ( pe) )
		goto _test_eof1071;
case 1071:
#line 23848 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 40: goto st1072;
		case 93: goto tr1270;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1072:
	if ( ++( p) == ( pe) )
		goto _test_eof1072;
case 1072:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 35: goto tr1273;
		case 47: goto tr1273;
		case 72: goto tr1274;
		case 93: goto tr1270;
		case 104: goto tr1274;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
tr1273:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1073;
st1073:
	if ( ++( p) == ( pe) )
		goto _test_eof1073;
case 1073:
#line 23882 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 41: goto tr1276;
		case 93: goto tr1277;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1073;
tr1276:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1917;
st1917:
	if ( ++( p) == ( pe) )
		goto _test_eof1917;
case 1917:
#line 23904 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2455;
		case 32: goto tr2455;
		case 93: goto tr1270;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2455;
	goto st1068;
tr1277:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1074;
st1074:
	if ( ++( p) == ( pe) )
		goto _test_eof1074;
case 1074:
#line 23921 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 40: goto st1075;
		case 41: goto tr1276;
		case 93: goto tr1277;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1073;
st1075:
	if ( ++( p) == ( pe) )
		goto _test_eof1075;
case 1075:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr998;
		case 41: goto tr1280;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto tr998;
	goto tr1279;
tr1280:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1918;
st1918:
	if ( ++( p) == ( pe) )
		goto _test_eof1918;
case 1918:
#line 23960 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2455;
		case 10: goto tr2455;
		case 13: goto tr2455;
		case 41: goto tr1002;
	}
	goto st819;
tr1274:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1076;
st1076:
	if ( ++( p) == ( pe) )
		goto _test_eof1076;
case 1076:
#line 23976 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 84: goto st1077;
		case 93: goto tr1270;
		case 116: goto st1077;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1077:
	if ( ++( p) == ( pe) )
		goto _test_eof1077;
case 1077:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 84: goto st1078;
		case 93: goto tr1270;
		case 116: goto st1078;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1078:
	if ( ++( p) == ( pe) )
		goto _test_eof1078;
case 1078:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 80: goto st1079;
		case 93: goto tr1270;
		case 112: goto st1079;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1079:
	if ( ++( p) == ( pe) )
		goto _test_eof1079;
case 1079:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 58: goto st1080;
		case 83: goto st1083;
		case 93: goto tr1270;
		case 115: goto st1083;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1080:
	if ( ++( p) == ( pe) )
		goto _test_eof1080;
case 1080:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 47: goto st1081;
		case 93: goto tr1270;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1081:
	if ( ++( p) == ( pe) )
		goto _test_eof1081;
case 1081:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 47: goto st1082;
		case 93: goto tr1270;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1082:
	if ( ++( p) == ( pe) )
		goto _test_eof1082;
case 1082:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 93: goto tr1277;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1073;
st1083:
	if ( ++( p) == ( pe) )
		goto _test_eof1083;
case 1083:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 58: goto st1080;
		case 93: goto tr1270;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1068;
st1084:
	if ( ++( p) == ( pe) )
		goto _test_eof1084;
case 1084:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 58: goto st1063;
		case 93: goto tr981;
	}
	goto st803;
tr2446:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1085;
st1085:
	if ( ++( p) == ( pe) )
		goto _test_eof1085;
case 1085:
#line 24101 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1288;
	}
	goto st803;
tr2447:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1086;
st1086:
	if ( ++( p) == ( pe) )
		goto _test_eof1086;
case 1086:
#line 24117 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st1087;
		case 93: goto tr981;
		case 111: goto st1087;
	}
	goto st803;
st1087:
	if ( ++( p) == ( pe) )
		goto _test_eof1087;
case 1087:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 68: goto st1088;
		case 93: goto tr981;
		case 100: goto st1088;
	}
	goto st803;
st1088:
	if ( ++( p) == ( pe) )
		goto _test_eof1088;
case 1088:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1089;
		case 93: goto tr981;
		case 116: goto st1089;
	}
	goto st803;
st1089:
	if ( ++( p) == ( pe) )
		goto _test_eof1089;
case 1089:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st1090;
		case 93: goto tr981;
		case 101: goto st1090;
	}
	goto st803;
st1090:
	if ( ++( p) == ( pe) )
		goto _test_eof1090;
case 1090:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 88: goto st1091;
		case 93: goto tr981;
		case 120: goto st1091;
	}
	goto st803;
st1091:
	if ( ++( p) == ( pe) )
		goto _test_eof1091;
case 1091:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1092;
		case 93: goto tr981;
		case 116: goto st1092;
	}
	goto st803;
st1092:
	if ( ++( p) == ( pe) )
		goto _test_eof1092;
case 1092:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1295;
	}
	goto st803;
tr1295:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1919;
st1919:
	if ( ++( p) == ( pe) )
		goto _test_eof1919;
case 1919:
#line 24211 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1297;
		case 9: goto st1093;
		case 10: goto tr1297;
		case 32: goto st1093;
	}
	goto tr2456;
st1093:
	if ( ++( p) == ( pe) )
		goto _test_eof1093;
case 1093:
	switch( (*( p)) ) {
		case 0: goto tr1297;
		case 9: goto st1093;
		case 10: goto tr1297;
		case 32: goto st1093;
	}
	goto tr1296;
tr2448:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1094;
st1094:
	if ( ++( p) == ( pe) )
		goto _test_eof1094;
case 1094:
#line 24238 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 85: goto st1095;
		case 93: goto tr981;
		case 117: goto st1095;
	}
	goto st803;
st1095:
	if ( ++( p) == ( pe) )
		goto _test_eof1095;
case 1095:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st1096;
		case 93: goto tr981;
		case 111: goto st1096;
	}
	goto st803;
st1096:
	if ( ++( p) == ( pe) )
		goto _test_eof1096;
case 1096:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1097;
		case 93: goto tr981;
		case 116: goto st1097;
	}
	goto st803;
st1097:
	if ( ++( p) == ( pe) )
		goto _test_eof1097;
case 1097:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st1098;
		case 93: goto tr981;
		case 101: goto st1098;
	}
	goto st803;
st1098:
	if ( ++( p) == ( pe) )
		goto _test_eof1098;
case 1098:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr304;
	}
	goto st803;
tr2449:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1099;
st1099:
	if ( ++( p) == ( pe) )
		goto _test_eof1099;
case 1099:
#line 24306 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st1100;
		case 85: goto st1107;
		case 93: goto tr1305;
		case 112: goto st1100;
		case 117: goto st1107;
	}
	goto st803;
st1100:
	if ( ++( p) == ( pe) )
		goto _test_eof1100;
case 1100:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st1101;
		case 93: goto tr981;
		case 111: goto st1101;
	}
	goto st803;
st1101:
	if ( ++( p) == ( pe) )
		goto _test_eof1101;
case 1101:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 73: goto st1102;
		case 93: goto tr981;
		case 105: goto st1102;
	}
	goto st803;
st1102:
	if ( ++( p) == ( pe) )
		goto _test_eof1102;
case 1102:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st1103;
		case 93: goto tr981;
		case 108: goto st1103;
	}
	goto st803;
st1103:
	if ( ++( p) == ( pe) )
		goto _test_eof1103;
case 1103:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st1104;
		case 93: goto tr981;
		case 101: goto st1104;
	}
	goto st803;
st1104:
	if ( ++( p) == ( pe) )
		goto _test_eof1104;
case 1104:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st1105;
		case 93: goto tr981;
		case 114: goto st1105;
	}
	goto st803;
st1105:
	if ( ++( p) == ( pe) )
		goto _test_eof1105;
case 1105:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 83: goto st1106;
		case 93: goto tr1312;
		case 115: goto st1106;
	}
	goto st803;
st1106:
	if ( ++( p) == ( pe) )
		goto _test_eof1106;
case 1106:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1312;
	}
	goto st803;
st1107:
	if ( ++( p) == ( pe) )
		goto _test_eof1107;
case 1107:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 66: goto st1108;
		case 80: goto st1109;
		case 93: goto tr981;
		case 98: goto st1108;
		case 112: goto st1109;
	}
	goto st803;
st1108:
	if ( ++( p) == ( pe) )
		goto _test_eof1108;
case 1108:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1315;
	}
	goto st803;
st1109:
	if ( ++( p) == ( pe) )
		goto _test_eof1109;
case 1109:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1316;
	}
	goto st803;
tr2450:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1110;
st1110:
	if ( ++( p) == ( pe) )
		goto _test_eof1110;
case 1110:
#line 24452 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st1111;
		case 66: goto st1114;
		case 68: goto st902;
		case 72: goto st1117;
		case 78: goto st1119;
		case 82: goto st902;
		case 93: goto tr981;
		case 97: goto st1111;
		case 98: goto st1114;
		case 100: goto st902;
		case 104: goto st1117;
		case 110: goto st1119;
		case 114: goto st902;
	}
	goto st803;
st1111:
	if ( ++( p) == ( pe) )
		goto _test_eof1111;
case 1111:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 66: goto st1112;
		case 93: goto tr981;
		case 98: goto st1112;
	}
	goto st803;
st1112:
	if ( ++( p) == ( pe) )
		goto _test_eof1112;
case 1112:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st1113;
		case 93: goto tr981;
		case 108: goto st1113;
	}
	goto st803;
st1113:
	if ( ++( p) == ( pe) )
		goto _test_eof1113;
case 1113:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st902;
		case 93: goto tr981;
		case 101: goto st902;
	}
	goto st803;
st1114:
	if ( ++( p) == ( pe) )
		goto _test_eof1114;
case 1114:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 79: goto st1115;
		case 93: goto tr981;
		case 111: goto st1115;
	}
	goto st803;
st1115:
	if ( ++( p) == ( pe) )
		goto _test_eof1115;
case 1115:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 68: goto st1116;
		case 93: goto tr981;
		case 100: goto st1116;
	}
	goto st803;
st1116:
	if ( ++( p) == ( pe) )
		goto _test_eof1116;
case 1116:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 89: goto st902;
		case 93: goto tr981;
		case 121: goto st902;
	}
	goto st803;
st1117:
	if ( ++( p) == ( pe) )
		goto _test_eof1117;
case 1117:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 69: goto st1118;
		case 93: goto tr257;
		case 101: goto st1118;
	}
	goto st803;
st1118:
	if ( ++( p) == ( pe) )
		goto _test_eof1118;
case 1118:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 65: goto st1058;
		case 93: goto tr981;
		case 97: goto st1058;
	}
	goto st803;
st1119:
	if ( ++( p) == ( pe) )
		goto _test_eof1119;
case 1119:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr1326;
	}
	goto st803;
tr2451:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1120;
st1120:
	if ( ++( p) == ( pe) )
		goto _test_eof1120;
case 1120:
#line 24595 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 82: goto st1121;
		case 93: goto tr1328;
		case 114: goto st1121;
	}
	goto st803;
st1121:
	if ( ++( p) == ( pe) )
		goto _test_eof1121;
case 1121:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 76: goto st1122;
		case 93: goto tr981;
		case 108: goto st1122;
	}
	goto st803;
st1122:
	if ( ++( p) == ( pe) )
		goto _test_eof1122;
case 1122:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1123;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1123;
		case 61: goto st1124;
		case 93: goto st1218;
	}
	goto st803;
st1123:
	if ( ++( p) == ( pe) )
		goto _test_eof1123;
case 1123:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1123;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1123;
		case 61: goto st1124;
		case 93: goto tr981;
	}
	goto st803;
st1124:
	if ( ++( p) == ( pe) )
		goto _test_eof1124;
case 1124:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1124;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1124;
		case 34: goto st1125;
		case 35: goto tr1334;
		case 39: goto st1176;
		case 47: goto tr1334;
		case 72: goto tr1336;
		case 93: goto tr981;
		case 104: goto tr1336;
	}
	goto st803;
st1125:
	if ( ++( p) == ( pe) )
		goto _test_eof1125;
case 1125:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 35: goto tr1337;
		case 47: goto tr1337;
		case 72: goto tr1338;
		case 93: goto tr981;
		case 104: goto tr1338;
	}
	goto st803;
tr1337:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1126;
st1126:
	if ( ++( p) == ( pe) )
		goto _test_eof1126;
case 1126:
#line 24688 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 34: goto tr1340;
		case 93: goto tr1341;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1126;
tr1340:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1127;
st1127:
	if ( ++( p) == ( pe) )
		goto _test_eof1127;
case 1127:
#line 24708 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1127;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1127;
		case 93: goto tr1343;
	}
	goto st803;
tr1343:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1128;
tr1401:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1128;
st1128:
	if ( ++( p) == ( pe) )
		goto _test_eof1128;
case 1128:
#line 24732 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1136;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1136;
		case 40: goto tr1346;
	}
	goto tr1344;
tr1344:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1129;
st1129:
	if ( ++( p) == ( pe) )
		goto _test_eof1129;
case 1129:
#line 24750 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr1348;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr1348;
		case 91: goto tr1349;
	}
	goto st1129;
tr1348:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1130;
st1130:
	if ( ++( p) == ( pe) )
		goto _test_eof1130;
case 1130:
#line 24768 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto st1130;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto st1130;
		case 91: goto st1131;
	}
	goto st1129;
tr1349:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1131;
st1131:
	if ( ++( p) == ( pe) )
		goto _test_eof1131;
case 1131:
#line 24786 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr1348;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr1348;
		case 47: goto st1132;
		case 91: goto tr1349;
	}
	goto st1129;
st1132:
	if ( ++( p) == ( pe) )
		goto _test_eof1132;
case 1132:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr1348;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr1348;
		case 85: goto st1133;
		case 91: goto tr1349;
		case 117: goto st1133;
	}
	goto st1129;
st1133:
	if ( ++( p) == ( pe) )
		goto _test_eof1133;
case 1133:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr1348;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr1348;
		case 82: goto st1134;
		case 91: goto tr1349;
		case 114: goto st1134;
	}
	goto st1129;
st1134:
	if ( ++( p) == ( pe) )
		goto _test_eof1134;
case 1134:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr1348;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr1348;
		case 76: goto st1135;
		case 91: goto tr1349;
		case 108: goto st1135;
	}
	goto st1129;
st1135:
	if ( ++( p) == ( pe) )
		goto _test_eof1135;
case 1135:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr1348;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto tr1348;
		case 91: goto tr1349;
		case 93: goto tr1356;
	}
	goto st1129;
tr1436:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1136;
st1136:
	if ( ++( p) == ( pe) )
		goto _test_eof1136;
case 1136:
#line 24864 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto st1136;
		case 10: goto tr250;
		case 13: goto tr250;
		case 32: goto st1136;
	}
	goto tr1344;
tr1346:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1137;
st1137:
	if ( ++( p) == ( pe) )
		goto _test_eof1137;
case 1137:
#line 24881 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 35: goto tr1357;
		case 47: goto tr1357;
		case 72: goto tr1358;
		case 91: goto tr1349;
		case 104: goto tr1358;
	}
	goto st1129;
tr1384:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1138;
tr1357:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1138;
st1138:
	if ( ++( p) == ( pe) )
		goto _test_eof1138;
case 1138:
#line 24907 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 41: goto tr1360;
		case 91: goto tr1361;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1129;
	goto st1138;
tr1360:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1920;
tr1385:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1920;
st1920:
	if ( ++( p) == ( pe) )
		goto _test_eof1920;
case 1920:
#line 24942 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2455;
		case 9: goto tr1348;
		case 10: goto tr2455;
		case 13: goto tr2455;
		case 32: goto tr1348;
		case 91: goto tr1349;
	}
	goto st1129;
tr1361:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1139;
st1139:
	if ( ++( p) == ( pe) )
		goto _test_eof1139;
case 1139:
#line 24960 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 41: goto tr1360;
		case 47: goto st1140;
		case 91: goto tr1361;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1129;
	goto st1138;
st1140:
	if ( ++( p) == ( pe) )
		goto _test_eof1140;
case 1140:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 41: goto tr1360;
		case 85: goto st1141;
		case 91: goto tr1361;
		case 117: goto st1141;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1129;
	goto st1138;
st1141:
	if ( ++( p) == ( pe) )
		goto _test_eof1141;
case 1141:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 41: goto tr1360;
		case 82: goto st1142;
		case 91: goto tr1361;
		case 114: goto st1142;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1129;
	goto st1138;
st1142:
	if ( ++( p) == ( pe) )
		goto _test_eof1142;
case 1142:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 41: goto tr1360;
		case 76: goto st1143;
		case 91: goto tr1361;
		case 108: goto st1143;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1129;
	goto st1138;
st1143:
	if ( ++( p) == ( pe) )
		goto _test_eof1143;
case 1143:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 41: goto tr1360;
		case 91: goto tr1361;
		case 93: goto tr1366;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1129;
	goto st1138;
tr1358:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1144;
st1144:
	if ( ++( p) == ( pe) )
		goto _test_eof1144;
case 1144:
#line 25053 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 84: goto st1145;
		case 91: goto tr1349;
		case 116: goto st1145;
	}
	goto st1129;
st1145:
	if ( ++( p) == ( pe) )
		goto _test_eof1145;
case 1145:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 84: goto st1146;
		case 91: goto tr1349;
		case 116: goto st1146;
	}
	goto st1129;
st1146:
	if ( ++( p) == ( pe) )
		goto _test_eof1146;
case 1146:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 80: goto st1147;
		case 91: goto tr1349;
		case 112: goto st1147;
	}
	goto st1129;
st1147:
	if ( ++( p) == ( pe) )
		goto _test_eof1147;
case 1147:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 58: goto st1148;
		case 83: goto st1151;
		case 91: goto tr1349;
		case 115: goto st1151;
	}
	goto st1129;
st1148:
	if ( ++( p) == ( pe) )
		goto _test_eof1148;
case 1148:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 47: goto st1149;
		case 91: goto tr1349;
	}
	goto st1129;
st1149:
	if ( ++( p) == ( pe) )
		goto _test_eof1149;
case 1149:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 47: goto st1150;
		case 91: goto tr1349;
	}
	goto st1129;
st1150:
	if ( ++( p) == ( pe) )
		goto _test_eof1150;
case 1150:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 91: goto tr1361;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1129;
	goto st1138;
st1151:
	if ( ++( p) == ( pe) )
		goto _test_eof1151;
case 1151:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1348;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1348;
		case 58: goto st1148;
		case 91: goto tr1349;
	}
	goto st1129;
tr1341:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1152;
st1152:
	if ( ++( p) == ( pe) )
		goto _test_eof1152;
case 1152:
#line 25176 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 40: goto st1155;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
st1153:
	if ( ++( p) == ( pe) )
		goto _test_eof1153;
case 1153:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 32: goto tr250;
		case 34: goto tr1375;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st1153;
tr1375:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1154;
st1154:
	if ( ++( p) == ( pe) )
		goto _test_eof1154;
case 1154:
#line 25206 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1154;
		case 32: goto st1154;
		case 93: goto st1136;
	}
	goto tr250;
st1155:
	if ( ++( p) == ( pe) )
		goto _test_eof1155;
case 1155:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 35: goto tr1378;
		case 47: goto tr1378;
		case 72: goto tr1379;
		case 104: goto tr1379;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
tr1378:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1156;
st1156:
	if ( ++( p) == ( pe) )
		goto _test_eof1156;
case 1156:
#line 25237 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1381;
		case 41: goto tr1382;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1156;
tr1381:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1157;
st1157:
	if ( ++( p) == ( pe) )
		goto _test_eof1157;
case 1157:
#line 25255 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1154;
		case 32: goto st1154;
		case 41: goto tr986;
		case 93: goto st1158;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st806;
tr1441:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1158;
st1158:
	if ( ++( p) == ( pe) )
		goto _test_eof1158;
case 1158:
#line 25274 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1136;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1136;
		case 41: goto tr1385;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr1344;
	goto tr1384;
tr1382:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1921;
st1921:
	if ( ++( p) == ( pe) )
		goto _test_eof1921;
case 1921:
#line 25298 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2455;
		case 32: goto tr2455;
		case 34: goto tr1375;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2455;
	goto st1153;
tr1379:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1159;
st1159:
	if ( ++( p) == ( pe) )
		goto _test_eof1159;
case 1159:
#line 25315 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 84: goto st1160;
		case 116: goto st1160;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
st1160:
	if ( ++( p) == ( pe) )
		goto _test_eof1160;
case 1160:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 84: goto st1161;
		case 116: goto st1161;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
st1161:
	if ( ++( p) == ( pe) )
		goto _test_eof1161;
case 1161:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 80: goto st1162;
		case 112: goto st1162;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
st1162:
	if ( ++( p) == ( pe) )
		goto _test_eof1162;
case 1162:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 58: goto st1163;
		case 83: goto st1166;
		case 115: goto st1166;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
st1163:
	if ( ++( p) == ( pe) )
		goto _test_eof1163;
case 1163:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 47: goto st1164;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
st1164:
	if ( ++( p) == ( pe) )
		goto _test_eof1164;
case 1164:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 47: goto st1165;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
st1165:
	if ( ++( p) == ( pe) )
		goto _test_eof1165;
case 1165:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1381;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1156;
st1166:
	if ( ++( p) == ( pe) )
		goto _test_eof1166;
case 1166:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1375;
		case 58: goto st1163;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1153;
tr1338:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1167;
st1167:
	if ( ++( p) == ( pe) )
		goto _test_eof1167;
case 1167:
#line 25428 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1168;
		case 93: goto tr981;
		case 116: goto st1168;
	}
	goto st803;
st1168:
	if ( ++( p) == ( pe) )
		goto _test_eof1168;
case 1168:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1169;
		case 93: goto tr981;
		case 116: goto st1169;
	}
	goto st803;
st1169:
	if ( ++( p) == ( pe) )
		goto _test_eof1169;
case 1169:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st1170;
		case 93: goto tr981;
		case 112: goto st1170;
	}
	goto st803;
st1170:
	if ( ++( p) == ( pe) )
		goto _test_eof1170;
case 1170:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 58: goto st1171;
		case 83: goto st1174;
		case 93: goto tr981;
		case 115: goto st1174;
	}
	goto st803;
st1171:
	if ( ++( p) == ( pe) )
		goto _test_eof1171;
case 1171:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1172;
		case 93: goto tr981;
	}
	goto st803;
st1172:
	if ( ++( p) == ( pe) )
		goto _test_eof1172;
case 1172:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1173;
		case 93: goto tr981;
	}
	goto st803;
st1173:
	if ( ++( p) == ( pe) )
		goto _test_eof1173;
case 1173:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1341;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1126;
st1174:
	if ( ++( p) == ( pe) )
		goto _test_eof1174;
case 1174:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 58: goto st1171;
		case 93: goto tr981;
	}
	goto st803;
tr1334:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1175;
st1175:
	if ( ++( p) == ( pe) )
		goto _test_eof1175;
case 1175:
#line 25536 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1340;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1340;
		case 93: goto tr1401;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1175;
st1176:
	if ( ++( p) == ( pe) )
		goto _test_eof1176;
case 1176:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 35: goto tr1402;
		case 47: goto tr1402;
		case 72: goto tr1403;
		case 93: goto tr981;
		case 104: goto tr1403;
	}
	goto st803;
tr1402:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1177;
st1177:
	if ( ++( p) == ( pe) )
		goto _test_eof1177;
case 1177:
#line 25571 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 39: goto tr1340;
		case 93: goto tr1405;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1177;
tr1405:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1178;
st1178:
	if ( ++( p) == ( pe) )
		goto _test_eof1178;
case 1178:
#line 25591 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1375;
		case 40: goto st1180;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
st1179:
	if ( ++( p) == ( pe) )
		goto _test_eof1179;
case 1179:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 32: goto tr250;
		case 39: goto tr1375;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st1179;
st1180:
	if ( ++( p) == ( pe) )
		goto _test_eof1180;
case 1180:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 35: goto tr1408;
		case 39: goto tr1375;
		case 47: goto tr1408;
		case 72: goto tr1409;
		case 104: goto tr1409;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
tr1408:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1181;
st1181:
	if ( ++( p) == ( pe) )
		goto _test_eof1181;
case 1181:
#line 25637 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1381;
		case 41: goto tr1411;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1181;
tr1411:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1922;
st1922:
	if ( ++( p) == ( pe) )
		goto _test_eof1922;
case 1922:
#line 25659 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2455;
		case 32: goto tr2455;
		case 39: goto tr1375;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr2455;
	goto st1179;
tr1409:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1182;
st1182:
	if ( ++( p) == ( pe) )
		goto _test_eof1182;
case 1182:
#line 25676 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1375;
		case 84: goto st1183;
		case 116: goto st1183;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
st1183:
	if ( ++( p) == ( pe) )
		goto _test_eof1183;
case 1183:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1375;
		case 84: goto st1184;
		case 116: goto st1184;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
st1184:
	if ( ++( p) == ( pe) )
		goto _test_eof1184;
case 1184:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1375;
		case 80: goto st1185;
		case 112: goto st1185;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
st1185:
	if ( ++( p) == ( pe) )
		goto _test_eof1185;
case 1185:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1375;
		case 58: goto st1186;
		case 83: goto st1189;
		case 115: goto st1189;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
st1186:
	if ( ++( p) == ( pe) )
		goto _test_eof1186;
case 1186:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1375;
		case 47: goto st1187;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
st1187:
	if ( ++( p) == ( pe) )
		goto _test_eof1187;
case 1187:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1375;
		case 47: goto st1188;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
st1188:
	if ( ++( p) == ( pe) )
		goto _test_eof1188;
case 1188:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1381;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1181;
st1189:
	if ( ++( p) == ( pe) )
		goto _test_eof1189;
case 1189:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 39: goto tr1375;
		case 58: goto st1186;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1179;
tr1403:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1190;
st1190:
	if ( ++( p) == ( pe) )
		goto _test_eof1190;
case 1190:
#line 25789 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1191;
		case 93: goto tr981;
		case 116: goto st1191;
	}
	goto st803;
st1191:
	if ( ++( p) == ( pe) )
		goto _test_eof1191;
case 1191:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1192;
		case 93: goto tr981;
		case 116: goto st1192;
	}
	goto st803;
st1192:
	if ( ++( p) == ( pe) )
		goto _test_eof1192;
case 1192:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st1193;
		case 93: goto tr981;
		case 112: goto st1193;
	}
	goto st803;
st1193:
	if ( ++( p) == ( pe) )
		goto _test_eof1193;
case 1193:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 58: goto st1194;
		case 83: goto st1197;
		case 93: goto tr981;
		case 115: goto st1197;
	}
	goto st803;
st1194:
	if ( ++( p) == ( pe) )
		goto _test_eof1194;
case 1194:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1195;
		case 93: goto tr981;
	}
	goto st803;
st1195:
	if ( ++( p) == ( pe) )
		goto _test_eof1195;
case 1195:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1196;
		case 93: goto tr981;
	}
	goto st803;
st1196:
	if ( ++( p) == ( pe) )
		goto _test_eof1196;
case 1196:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1405;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1177;
st1197:
	if ( ++( p) == ( pe) )
		goto _test_eof1197;
case 1197:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 58: goto st1194;
		case 93: goto tr981;
	}
	goto st803;
tr1336:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1198;
st1198:
	if ( ++( p) == ( pe) )
		goto _test_eof1198;
case 1198:
#line 25897 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1199;
		case 93: goto tr981;
		case 116: goto st1199;
	}
	goto st803;
st1199:
	if ( ++( p) == ( pe) )
		goto _test_eof1199;
case 1199:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 84: goto st1200;
		case 93: goto tr981;
		case 116: goto st1200;
	}
	goto st803;
st1200:
	if ( ++( p) == ( pe) )
		goto _test_eof1200;
case 1200:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 80: goto st1201;
		case 93: goto tr981;
		case 112: goto st1201;
	}
	goto st803;
st1201:
	if ( ++( p) == ( pe) )
		goto _test_eof1201;
case 1201:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 58: goto st1202;
		case 83: goto st1217;
		case 93: goto tr981;
		case 115: goto st1217;
	}
	goto st803;
st1202:
	if ( ++( p) == ( pe) )
		goto _test_eof1202;
case 1202:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1203;
		case 93: goto tr981;
	}
	goto st803;
st1203:
	if ( ++( p) == ( pe) )
		goto _test_eof1203;
case 1203:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1204;
		case 93: goto tr981;
	}
	goto st803;
st1204:
	if ( ++( p) == ( pe) )
		goto _test_eof1204;
case 1204:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st803;
		case 93: goto tr1433;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1175;
tr1433:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1205;
st1205:
	if ( ++( p) == ( pe) )
		goto _test_eof1205;
case 1205:
#line 25993 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 40: goto st1207;
		case 93: goto tr1436;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
st1206:
	if ( ++( p) == ( pe) )
		goto _test_eof1206;
case 1206:
	switch( (*( p)) ) {
		case 0: goto tr250;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 93: goto tr1436;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr250;
	goto st1206;
st1207:
	if ( ++( p) == ( pe) )
		goto _test_eof1207;
case 1207:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 35: goto tr1437;
		case 47: goto tr1437;
		case 72: goto tr1438;
		case 93: goto tr1436;
		case 104: goto tr1438;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
tr1437:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1208;
st1208:
	if ( ++( p) == ( pe) )
		goto _test_eof1208;
case 1208:
#line 26042 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 41: goto tr1440;
		case 93: goto tr1441;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1208;
tr1440:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 86 "ext/dtext/dtext.cpp.rl"
	{ g2 = p; }
#line 406 "ext/dtext/dtext.cpp.rl"
	{( act) = 55;}
	goto st1923;
st1923:
	if ( ++( p) == ( pe) )
		goto _test_eof1923;
case 1923:
#line 26065 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2455;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 93: goto tr1436;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr2455;
	goto st1206;
tr1438:
#line 85 "ext/dtext/dtext.cpp.rl"
	{ g1 = p; }
	goto st1209;
st1209:
	if ( ++( p) == ( pe) )
		goto _test_eof1209;
case 1209:
#line 26083 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 84: goto st1210;
		case 93: goto tr1436;
		case 116: goto st1210;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
st1210:
	if ( ++( p) == ( pe) )
		goto _test_eof1210;
case 1210:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 84: goto st1211;
		case 93: goto tr1436;
		case 116: goto st1211;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
st1211:
	if ( ++( p) == ( pe) )
		goto _test_eof1211;
case 1211:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 80: goto st1212;
		case 93: goto tr1436;
		case 112: goto st1212;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
st1212:
	if ( ++( p) == ( pe) )
		goto _test_eof1212;
case 1212:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 58: goto st1213;
		case 83: goto st1216;
		case 93: goto tr1436;
		case 115: goto st1216;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
st1213:
	if ( ++( p) == ( pe) )
		goto _test_eof1213;
case 1213:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 47: goto st1214;
		case 93: goto tr1436;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
st1214:
	if ( ++( p) == ( pe) )
		goto _test_eof1214;
case 1214:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 47: goto st1215;
		case 93: goto tr1436;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
st1215:
	if ( ++( p) == ( pe) )
		goto _test_eof1215;
case 1215:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 93: goto tr1441;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1208;
st1216:
	if ( ++( p) == ( pe) )
		goto _test_eof1216;
case 1216:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1375;
		case 32: goto tr1375;
		case 58: goto st1213;
		case 93: goto tr1436;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1206;
st1217:
	if ( ++( p) == ( pe) )
		goto _test_eof1217;
case 1217:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 58: goto st1202;
		case 93: goto tr981;
	}
	goto st803;
st1218:
	if ( ++( p) == ( pe) )
		goto _test_eof1218;
case 1218:
	switch( (*( p)) ) {
		case 9: goto st1218;
		case 32: goto st1218;
		case 35: goto tr1449;
		case 47: goto tr1449;
		case 72: goto tr1450;
		case 104: goto tr1450;
	}
	goto tr257;
tr1449:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1219;
st1219:
	if ( ++( p) == ( pe) )
		goto _test_eof1219;
case 1219:
#line 26229 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1452;
		case 32: goto tr1452;
		case 91: goto tr1453;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1219;
tr1452:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1220;
st1220:
	if ( ++( p) == ( pe) )
		goto _test_eof1220;
case 1220:
#line 26247 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1220;
		case 32: goto st1220;
		case 91: goto st1221;
	}
	goto tr257;
st1221:
	if ( ++( p) == ( pe) )
		goto _test_eof1221;
case 1221:
	if ( (*( p)) == 47 )
		goto st1222;
	goto tr257;
st1222:
	if ( ++( p) == ( pe) )
		goto _test_eof1222;
case 1222:
	switch( (*( p)) ) {
		case 85: goto st1223;
		case 117: goto st1223;
	}
	goto tr257;
st1223:
	if ( ++( p) == ( pe) )
		goto _test_eof1223;
case 1223:
	switch( (*( p)) ) {
		case 82: goto st1224;
		case 114: goto st1224;
	}
	goto tr257;
st1224:
	if ( ++( p) == ( pe) )
		goto _test_eof1224;
case 1224:
	switch( (*( p)) ) {
		case 76: goto st1225;
		case 108: goto st1225;
	}
	goto tr257;
st1225:
	if ( ++( p) == ( pe) )
		goto _test_eof1225;
case 1225:
	if ( (*( p)) == 93 )
		goto tr1460;
	goto tr257;
tr1453:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1226;
st1226:
	if ( ++( p) == ( pe) )
		goto _test_eof1226;
case 1226:
#line 26303 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1452;
		case 32: goto tr1452;
		case 47: goto st1227;
		case 91: goto tr1453;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1219;
st1227:
	if ( ++( p) == ( pe) )
		goto _test_eof1227;
case 1227:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1452;
		case 32: goto tr1452;
		case 85: goto st1228;
		case 91: goto tr1453;
		case 117: goto st1228;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1219;
st1228:
	if ( ++( p) == ( pe) )
		goto _test_eof1228;
case 1228:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1452;
		case 32: goto tr1452;
		case 82: goto st1229;
		case 91: goto tr1453;
		case 114: goto st1229;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1219;
st1229:
	if ( ++( p) == ( pe) )
		goto _test_eof1229;
case 1229:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1452;
		case 32: goto tr1452;
		case 76: goto st1230;
		case 91: goto tr1453;
		case 108: goto st1230;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1219;
st1230:
	if ( ++( p) == ( pe) )
		goto _test_eof1230;
case 1230:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1452;
		case 32: goto tr1452;
		case 91: goto tr1453;
		case 93: goto tr1460;
	}
	if ( 10 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1219;
tr1450:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1231;
st1231:
	if ( ++( p) == ( pe) )
		goto _test_eof1231;
case 1231:
#line 26381 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1232;
		case 116: goto st1232;
	}
	goto tr257;
st1232:
	if ( ++( p) == ( pe) )
		goto _test_eof1232;
case 1232:
	switch( (*( p)) ) {
		case 84: goto st1233;
		case 116: goto st1233;
	}
	goto tr257;
st1233:
	if ( ++( p) == ( pe) )
		goto _test_eof1233;
case 1233:
	switch( (*( p)) ) {
		case 80: goto st1234;
		case 112: goto st1234;
	}
	goto tr257;
st1234:
	if ( ++( p) == ( pe) )
		goto _test_eof1234;
case 1234:
	switch( (*( p)) ) {
		case 58: goto st1235;
		case 83: goto st1238;
		case 115: goto st1238;
	}
	goto tr257;
st1235:
	if ( ++( p) == ( pe) )
		goto _test_eof1235;
case 1235:
	if ( (*( p)) == 47 )
		goto st1236;
	goto tr257;
st1236:
	if ( ++( p) == ( pe) )
		goto _test_eof1236;
case 1236:
	if ( (*( p)) == 47 )
		goto st1237;
	goto tr257;
st1237:
	if ( ++( p) == ( pe) )
		goto _test_eof1237;
case 1237:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1219;
st1238:
	if ( ++( p) == ( pe) )
		goto _test_eof1238;
case 1238:
	if ( (*( p)) == 58 )
		goto st1235;
	goto tr257;
tr2452:
#line 83 "ext/dtext/dtext.cpp.rl"
	{ f1 = p; }
	goto st1239;
st1239:
	if ( ++( p) == ( pe) )
		goto _test_eof1239;
case 1239:
#line 26455 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr598;
		case 9: goto tr1473;
		case 10: goto tr600;
		case 13: goto tr600;
		case 32: goto tr1473;
		case 35: goto tr1475;
		case 58: goto tr1476;
		case 60: goto tr1477;
		case 62: goto tr1478;
		case 92: goto tr1479;
		case 93: goto tr981;
		case 124: goto tr1480;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr1474;
	goto tr1472;
tr1472:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1240;
st1240:
	if ( ++( p) == ( pe) )
		goto _test_eof1240;
case 1240:
#line 26481 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto tr1482;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto tr1482;
		case 35: goto tr1484;
		case 93: goto tr1485;
		case 124: goto tr1486;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1242;
	goto st1240;
tr1482:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1241;
st1241:
	if ( ++( p) == ( pe) )
		goto _test_eof1241;
case 1241:
#line 26503 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto st1241;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto st1241;
		case 35: goto st1243;
		case 93: goto tr1489;
		case 124: goto st1247;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1242;
	goto st1240;
tr1474:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1242;
st1242:
	if ( ++( p) == ( pe) )
		goto _test_eof1242;
case 1242:
#line 26525 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto st1242;
		case 93: goto tr981;
		case 124: goto st803;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 12 )
		goto st1242;
	goto st1240;
tr1484:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1243;
st1243:
	if ( ++( p) == ( pe) )
		goto _test_eof1243;
case 1243:
#line 26545 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto tr1482;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto tr1482;
		case 35: goto tr1484;
		case 93: goto tr1485;
		case 124: goto tr1486;
	}
	if ( (*( p)) > 12 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr1491;
	} else if ( (*( p)) >= 11 )
		goto st1242;
	goto st1240;
tr1491:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1244;
st1244:
	if ( ++( p) == ( pe) )
		goto _test_eof1244;
case 1244:
#line 26570 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1492;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1493;
		case 45: goto st1252;
		case 93: goto tr1496;
		case 95: goto st1252;
		case 124: goto tr1497;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1244;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1244;
	} else
		goto st1244;
	goto st803;
tr1492:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1245;
st1245:
	if ( ++( p) == ( pe) )
		goto _test_eof1245;
case 1245:
#line 26599 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1245;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1245;
		case 93: goto tr1489;
		case 124: goto st1247;
	}
	goto st803;
tr1489:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1246;
tr1485:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1246;
tr1496:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1246;
st1246:
	if ( ++( p) == ( pe) )
		goto _test_eof1246;
case 1246:
#line 26630 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st805;
		case 93: goto st1724;
	}
	goto tr257;
tr1486:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1247;
tr1497:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1247;
tr1500:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1247;
st1247:
	if ( ++( p) == ( pe) )
		goto _test_eof1247;
case 1247:
#line 26654 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr626;
		case 9: goto tr1500;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1500;
		case 93: goto tr1501;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto tr1499;
tr1499:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1248;
st1248:
	if ( ++( p) == ( pe) )
		goto _test_eof1248;
case 1248:
#line 26675 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto tr1503;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1503;
		case 93: goto tr1504;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1248;
tr1503:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1249;
st1249:
	if ( ++( p) == ( pe) )
		goto _test_eof1249;
case 1249:
#line 26696 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto st1249;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1249;
		case 93: goto tr1506;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1248;
tr1506:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1250;
tr1501:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1250;
tr1504:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1250;
st1250:
	if ( ++( p) == ( pe) )
		goto _test_eof1250;
case 1250:
#line 26731 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st805;
		case 93: goto st1726;
	}
	goto tr257;
tr1493:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1251;
st1251:
	if ( ++( p) == ( pe) )
		goto _test_eof1251;
case 1251:
#line 26745 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1245;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1251;
		case 45: goto st1252;
		case 93: goto tr1489;
		case 95: goto st1252;
		case 124: goto st1247;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1244;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1244;
	} else
		goto st1244;
	goto st803;
st1252:
	if ( ++( p) == ( pe) )
		goto _test_eof1252;
case 1252:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1252;
		case 45: goto st1252;
		case 93: goto tr981;
		case 95: goto st1252;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1244;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1244;
	} else
		goto st1244;
	goto st803;
tr1473:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1253;
st1253:
	if ( ++( p) == ( pe) )
		goto _test_eof1253;
case 1253:
#line 26796 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr598;
		case 9: goto tr1473;
		case 10: goto tr600;
		case 13: goto tr600;
		case 32: goto tr1473;
		case 58: goto tr1476;
		case 60: goto tr1477;
		case 62: goto tr1478;
		case 92: goto tr1479;
		case 93: goto tr981;
		case 124: goto tr1480;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto tr1474;
	goto tr1472;
tr1476:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1254;
st1254:
	if ( ++( p) == ( pe) )
		goto _test_eof1254;
case 1254:
#line 26821 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto tr1482;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto tr1482;
		case 35: goto tr1484;
		case 93: goto tr1485;
		case 124: goto tr1508;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1242;
	goto st1240;
tr1508:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1255;
st1255:
	if ( ++( p) == ( pe) )
		goto _test_eof1255;
case 1255:
#line 26843 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr626;
		case 9: goto tr1509;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1509;
		case 35: goto tr1510;
		case 93: goto tr1511;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto tr1499;
tr1512:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1256;
tr1509:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1256;
st1256:
	if ( ++( p) == ( pe) )
		goto _test_eof1256;
case 1256:
#line 26875 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr626;
		case 9: goto tr1512;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1512;
		case 35: goto tr1513;
		case 93: goto tr1514;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto tr1499;
tr1547:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1257;
tr1513:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1257;
tr1510:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1257;
st1257:
	if ( ++( p) == ( pe) )
		goto _test_eof1257;
case 1257:
#line 26907 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto tr1503;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1503;
		case 93: goto tr1504;
		case 124: goto st803;
	}
	if ( (*( p)) > 12 ) {
		if ( 65 <= (*( p)) && (*( p)) <= 90 )
			goto tr1515;
	} else if ( (*( p)) >= 11 )
		goto st803;
	goto st1248;
tr1515:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1258;
st1258:
	if ( ++( p) == ( pe) )
		goto _test_eof1258;
case 1258:
#line 26931 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto tr1516;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1517;
		case 45: goto st1262;
		case 93: goto tr1520;
		case 95: goto st1262;
		case 124: goto st803;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st803;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1258;
		} else if ( (*( p)) >= 65 )
			goto st1258;
	} else
		goto st1258;
	goto st1248;
tr1516:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1259;
st1259:
	if ( ++( p) == ( pe) )
		goto _test_eof1259;
case 1259:
#line 26965 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto st1259;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1259;
		case 93: goto tr1522;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1248;
tr1522:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1260;
tr1514:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1260;
tr1511:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1260;
tr1520:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1260;
tr1548:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1260;
st1260:
	if ( ++( p) == ( pe) )
		goto _test_eof1260;
case 1260:
#line 27020 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st805;
		case 93: goto st1728;
	}
	goto tr257;
tr1517:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1261;
st1261:
	if ( ++( p) == ( pe) )
		goto _test_eof1261;
case 1261:
#line 27036 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto st1259;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1261;
		case 45: goto st1262;
		case 93: goto tr1522;
		case 95: goto st1262;
		case 124: goto st803;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st803;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1258;
		} else if ( (*( p)) >= 65 )
			goto st1258;
	} else
		goto st1258;
	goto st1248;
st1262:
	if ( ++( p) == ( pe) )
		goto _test_eof1262;
case 1262:
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto tr1503;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1524;
		case 45: goto st1262;
		case 93: goto tr1504;
		case 95: goto st1262;
		case 124: goto st803;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st803;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1258;
		} else if ( (*( p)) >= 65 )
			goto st1258;
	} else
		goto st1258;
	goto st1248;
tr1524:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1263;
st1263:
	if ( ++( p) == ( pe) )
		goto _test_eof1263;
case 1263:
#line 27095 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto st1249;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1263;
		case 45: goto st1262;
		case 93: goto tr1506;
		case 95: goto st1262;
		case 124: goto st803;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st803;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1258;
		} else if ( (*( p)) >= 65 )
			goto st1258;
	} else
		goto st1258;
	goto st1248;
tr1477:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1264;
st1264:
	if ( ++( p) == ( pe) )
		goto _test_eof1264;
case 1264:
#line 27127 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto tr1482;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto tr1482;
		case 35: goto tr1484;
		case 93: goto tr1485;
		case 124: goto tr1526;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1242;
	goto st1240;
tr1526:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1265;
st1265:
	if ( ++( p) == ( pe) )
		goto _test_eof1265;
case 1265:
#line 27149 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr626;
		case 9: goto tr1500;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1500;
		case 62: goto tr1527;
		case 93: goto tr1501;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto tr1499;
tr1527:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1266;
st1266:
	if ( ++( p) == ( pe) )
		goto _test_eof1266;
case 1266:
#line 27171 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto tr1503;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1503;
		case 93: goto tr1504;
		case 95: goto st1267;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1248;
st1267:
	if ( ++( p) == ( pe) )
		goto _test_eof1267;
case 1267:
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto tr1503;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1503;
		case 60: goto st1268;
		case 93: goto tr1504;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1248;
st1268:
	if ( ++( p) == ( pe) )
		goto _test_eof1268;
case 1268:
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto tr1503;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1503;
		case 93: goto tr1504;
		case 124: goto st1269;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1248;
st1269:
	if ( ++( p) == ( pe) )
		goto _test_eof1269;
case 1269:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 62: goto st1270;
		case 93: goto tr981;
	}
	goto st803;
st1270:
	if ( ++( p) == ( pe) )
		goto _test_eof1270;
case 1270:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1532;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1532;
		case 35: goto tr1533;
		case 93: goto tr1485;
	}
	goto st803;
tr1532:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1271;
st1271:
	if ( ++( p) == ( pe) )
		goto _test_eof1271;
case 1271:
#line 27252 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1271;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1271;
		case 35: goto st1272;
		case 93: goto tr1489;
	}
	goto st803;
tr1533:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1272;
st1272:
	if ( ++( p) == ( pe) )
		goto _test_eof1272;
case 1272:
#line 27271 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
	}
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr1536;
	goto st803;
tr1536:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ c1 = p; }
	goto st1273;
st1273:
	if ( ++( p) == ( pe) )
		goto _test_eof1273;
case 1273:
#line 27289 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1537;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1538;
		case 45: goto st1276;
		case 93: goto tr1496;
		case 95: goto st1276;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1273;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1273;
	} else
		goto st1273;
	goto st803;
tr1537:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1274;
st1274:
	if ( ++( p) == ( pe) )
		goto _test_eof1274;
case 1274:
#line 27317 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1274;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1274;
		case 93: goto tr1489;
	}
	goto st803;
tr1538:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ c2 = p; }
	goto st1275;
st1275:
	if ( ++( p) == ( pe) )
		goto _test_eof1275;
case 1275:
#line 27335 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1274;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1275;
		case 45: goto st1276;
		case 93: goto tr1489;
		case 95: goto st1276;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1273;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1273;
	} else
		goto st1273;
	goto st803;
st1276:
	if ( ++( p) == ( pe) )
		goto _test_eof1276;
case 1276:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1276;
		case 45: goto st1276;
		case 93: goto tr981;
		case 95: goto st1276;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1273;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1273;
	} else
		goto st1273;
	goto st803;
tr1478:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1277;
st1277:
	if ( ++( p) == ( pe) )
		goto _test_eof1277;
case 1277:
#line 27385 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto tr1482;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto tr1482;
		case 35: goto tr1484;
		case 58: goto st1254;
		case 93: goto tr1485;
		case 124: goto tr1544;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1242;
	goto st1240;
tr1544:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1278;
st1278:
	if ( ++( p) == ( pe) )
		goto _test_eof1278;
case 1278:
#line 27408 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr626;
		case 9: goto tr1500;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1500;
		case 51: goto tr1545;
		case 93: goto tr1501;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto tr1499;
tr1545:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1279;
st1279:
	if ( ++( p) == ( pe) )
		goto _test_eof1279;
case 1279:
#line 27430 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto tr1546;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1546;
		case 35: goto tr1547;
		case 93: goto tr1548;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1248;
tr1546:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1280;
st1280:
	if ( ++( p) == ( pe) )
		goto _test_eof1280;
case 1280:
#line 27454 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st523;
		case 9: goto st1280;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1280;
		case 35: goto st1257;
		case 93: goto tr1522;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1248;
tr1479:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1281;
st1281:
	if ( ++( p) == ( pe) )
		goto _test_eof1281;
case 1281:
#line 27476 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto tr1482;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto tr1482;
		case 35: goto tr1484;
		case 93: goto tr1485;
		case 124: goto tr1551;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st1242;
	goto st1240;
tr1551:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1282;
st1282:
	if ( ++( p) == ( pe) )
		goto _test_eof1282;
case 1282:
#line 27498 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr626;
		case 9: goto tr1500;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1500;
		case 93: goto tr1501;
		case 124: goto st1283;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto tr1499;
st1283:
	if ( ++( p) == ( pe) )
		goto _test_eof1283;
case 1283:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1270;
		case 93: goto tr981;
	}
	goto st803;
tr1480:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1284;
st1284:
	if ( ++( p) == ( pe) )
		goto _test_eof1284;
case 1284:
#line 27531 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
		case 95: goto st1288;
		case 119: goto st1289;
		case 124: goto st1290;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1285;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1285;
	} else
		goto st1285;
	goto st803;
st1285:
	if ( ++( p) == ( pe) )
		goto _test_eof1285;
case 1285:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1557;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1557;
		case 35: goto tr1558;
		case 93: goto tr1485;
		case 124: goto tr1486;
	}
	goto st803;
tr1557:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1286;
st1286:
	if ( ++( p) == ( pe) )
		goto _test_eof1286;
case 1286:
#line 27573 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto st1286;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1286;
		case 35: goto st1287;
		case 93: goto tr1489;
		case 124: goto st1247;
	}
	goto st803;
tr1558:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1287;
st1287:
	if ( ++( p) == ( pe) )
		goto _test_eof1287;
case 1287:
#line 27593 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
	}
	if ( 65 <= (*( p)) && (*( p)) <= 90 )
		goto tr1491;
	goto st803;
st1288:
	if ( ++( p) == ( pe) )
		goto _test_eof1288;
case 1288:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
		case 124: goto st1285;
	}
	goto st803;
st1289:
	if ( ++( p) == ( pe) )
		goto _test_eof1289;
case 1289:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 9: goto tr1557;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1557;
		case 35: goto tr1558;
		case 93: goto tr1485;
		case 124: goto tr1508;
	}
	goto st803;
st1290:
	if ( ++( p) == ( pe) )
		goto _test_eof1290;
case 1290:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
		case 95: goto st1291;
	}
	goto st803;
st1291:
	if ( ++( p) == ( pe) )
		goto _test_eof1291;
case 1291:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 93: goto tr981;
		case 124: goto st1288;
	}
	goto st803;
tr1475:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1292;
st1292:
	if ( ++( p) == ( pe) )
		goto _test_eof1292;
case 1292:
#line 27662 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto tr1482;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto tr1482;
		case 35: goto tr1484;
		case 45: goto tr1562;
		case 93: goto tr1485;
		case 95: goto tr1562;
		case 124: goto tr1486;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st1242;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto tr1562;
		} else if ( (*( p)) >= 65 )
			goto tr1562;
	} else
		goto tr1562;
	goto st1240;
tr1562:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1293;
st1293:
	if ( ++( p) == ( pe) )
		goto _test_eof1293;
case 1293:
#line 27695 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st515;
		case 9: goto tr1482;
		case 10: goto st517;
		case 13: goto st517;
		case 32: goto tr1482;
		case 35: goto tr1484;
		case 45: goto st1293;
		case 93: goto tr1564;
		case 95: goto st1293;
		case 124: goto tr1565;
	}
	if ( (*( p)) < 48 ) {
		if ( 11 <= (*( p)) && (*( p)) <= 12 )
			goto st1242;
	} else if ( (*( p)) > 57 ) {
		if ( (*( p)) > 90 ) {
			if ( 97 <= (*( p)) && (*( p)) <= 122 )
				goto st1293;
		} else if ( (*( p)) >= 65 )
			goto st1293;
	} else
		goto st1293;
	goto st1240;
tr1564:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1294;
st1294:
	if ( ++( p) == ( pe) )
		goto _test_eof1294;
case 1294:
#line 27730 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st805;
		case 93: goto st1730;
	}
	goto tr257;
tr1565:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1295;
tr1567:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1295;
st1295:
	if ( ++( p) == ( pe) )
		goto _test_eof1295;
case 1295:
#line 27750 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr696;
		case 9: goto tr1567;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1567;
		case 93: goto tr1568;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto tr1566;
tr1566:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
	goto st1296;
st1296:
	if ( ++( p) == ( pe) )
		goto _test_eof1296;
case 1296:
#line 27771 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st571;
		case 9: goto tr1570;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto tr1570;
		case 93: goto tr1571;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1296;
tr1570:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
	goto st1297;
st1297:
	if ( ++( p) == ( pe) )
		goto _test_eof1297;
case 1297:
#line 27792 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto st571;
		case 9: goto st1297;
		case 10: goto tr257;
		case 13: goto tr257;
		case 32: goto st1297;
		case 93: goto tr1573;
		case 124: goto st803;
	}
	if ( 11 <= (*( p)) && (*( p)) <= 12 )
		goto st803;
	goto st1296;
tr1573:
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1298;
tr1568:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ d1 = p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1298;
tr1571:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ d2 = p; }
#line 84 "ext/dtext/dtext.cpp.rl"
	{ f2 = p; }
	goto st1298;
st1298:
	if ( ++( p) == ( pe) )
		goto _test_eof1298;
case 1298:
#line 27827 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 40: goto st805;
		case 93: goto st1732;
	}
	goto tr257;
tr2165:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1924;
st1924:
	if ( ++( p) == ( pe) )
		goto _test_eof1924;
case 1924:
#line 27847 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 123 )
		goto st575;
	goto tr2169;
tr2166:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1925;
st1925:
	if ( ++( p) == ( pe) )
		goto _test_eof1925;
case 1925:
#line 27863 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1299;
		case 65: goto st1313;
		case 66: goto st1336;
		case 67: goto st1338;
		case 69: goto st1345;
		case 72: goto tr2462;
		case 73: goto st1346;
		case 78: goto st1356;
		case 81: goto st251;
		case 83: goto st1363;
		case 84: goto st1379;
		case 85: goto st1381;
		case 97: goto st1313;
		case 98: goto st1336;
		case 99: goto st1338;
		case 101: goto st1345;
		case 104: goto tr2462;
		case 105: goto st1346;
		case 110: goto st1356;
		case 113: goto st251;
		case 115: goto st1363;
		case 116: goto st1379;
		case 117: goto st1381;
	}
	goto tr2169;
st1299:
	if ( ++( p) == ( pe) )
		goto _test_eof1299;
case 1299:
	switch( (*( p)) ) {
		case 66: goto st1300;
		case 69: goto st1301;
		case 73: goto st1302;
		case 81: goto st238;
		case 83: goto st1303;
		case 84: goto st314;
		case 85: goto st1312;
		case 98: goto st1300;
		case 101: goto st1301;
		case 105: goto st1302;
		case 113: goto st238;
		case 115: goto st1303;
		case 116: goto st314;
		case 117: goto st1312;
	}
	goto tr257;
st1300:
	if ( ++( p) == ( pe) )
		goto _test_eof1300;
case 1300:
	switch( (*( p)) ) {
		case 62: goto tr1026;
		case 76: goto st223;
		case 108: goto st223;
	}
	goto tr257;
st1301:
	if ( ++( p) == ( pe) )
		goto _test_eof1301;
case 1301:
	switch( (*( p)) ) {
		case 77: goto st1302;
		case 88: goto st233;
		case 109: goto st1302;
		case 120: goto st233;
	}
	goto tr257;
st1302:
	if ( ++( p) == ( pe) )
		goto _test_eof1302;
case 1302:
	if ( (*( p)) == 62 )
		goto tr1042;
	goto tr257;
st1303:
	if ( ++( p) == ( pe) )
		goto _test_eof1303;
case 1303:
	switch( (*( p)) ) {
		case 62: goto tr1054;
		case 80: goto st307;
		case 84: goto st1304;
		case 85: goto st1309;
		case 112: goto st307;
		case 116: goto st1304;
		case 117: goto st1309;
	}
	goto tr257;
st1304:
	if ( ++( p) == ( pe) )
		goto _test_eof1304;
case 1304:
	switch( (*( p)) ) {
		case 82: goto st1305;
		case 114: goto st1305;
	}
	goto tr257;
st1305:
	if ( ++( p) == ( pe) )
		goto _test_eof1305;
case 1305:
	switch( (*( p)) ) {
		case 79: goto st1306;
		case 111: goto st1306;
	}
	goto tr257;
st1306:
	if ( ++( p) == ( pe) )
		goto _test_eof1306;
case 1306:
	switch( (*( p)) ) {
		case 78: goto st1307;
		case 110: goto st1307;
	}
	goto tr257;
st1307:
	if ( ++( p) == ( pe) )
		goto _test_eof1307;
case 1307:
	switch( (*( p)) ) {
		case 71: goto st1308;
		case 103: goto st1308;
	}
	goto tr257;
st1308:
	if ( ++( p) == ( pe) )
		goto _test_eof1308;
case 1308:
	if ( (*( p)) == 62 )
		goto tr1026;
	goto tr257;
st1309:
	if ( ++( p) == ( pe) )
		goto _test_eof1309;
case 1309:
	switch( (*( p)) ) {
		case 66: goto st1310;
		case 80: goto st1311;
		case 98: goto st1310;
		case 112: goto st1311;
	}
	goto tr257;
st1310:
	if ( ++( p) == ( pe) )
		goto _test_eof1310;
case 1310:
	if ( (*( p)) == 62 )
		goto tr1587;
	goto tr257;
st1311:
	if ( ++( p) == ( pe) )
		goto _test_eof1311;
case 1311:
	if ( (*( p)) == 62 )
		goto tr1588;
	goto tr257;
st1312:
	if ( ++( p) == ( pe) )
		goto _test_eof1312;
case 1312:
	if ( (*( p)) == 62 )
		goto tr1076;
	goto tr257;
st1313:
	if ( ++( p) == ( pe) )
		goto _test_eof1313;
case 1313:
	switch( (*( p)) ) {
		case 9: goto st1314;
		case 32: goto st1314;
	}
	goto tr257;
st1314:
	if ( ++( p) == ( pe) )
		goto _test_eof1314;
case 1314:
	switch( (*( p)) ) {
		case 9: goto st1314;
		case 32: goto st1314;
		case 72: goto st1315;
		case 104: goto st1315;
	}
	goto tr257;
st1315:
	if ( ++( p) == ( pe) )
		goto _test_eof1315;
case 1315:
	switch( (*( p)) ) {
		case 82: goto st1316;
		case 114: goto st1316;
	}
	goto tr257;
st1316:
	if ( ++( p) == ( pe) )
		goto _test_eof1316;
case 1316:
	switch( (*( p)) ) {
		case 69: goto st1317;
		case 101: goto st1317;
	}
	goto tr257;
st1317:
	if ( ++( p) == ( pe) )
		goto _test_eof1317;
case 1317:
	switch( (*( p)) ) {
		case 70: goto st1318;
		case 102: goto st1318;
	}
	goto tr257;
st1318:
	if ( ++( p) == ( pe) )
		goto _test_eof1318;
case 1318:
	if ( (*( p)) == 61 )
		goto st1319;
	goto tr257;
st1319:
	if ( ++( p) == ( pe) )
		goto _test_eof1319;
case 1319:
	if ( (*( p)) == 34 )
		goto st1320;
	goto tr257;
st1320:
	if ( ++( p) == ( pe) )
		goto _test_eof1320;
case 1320:
	switch( (*( p)) ) {
		case 35: goto tr1596;
		case 47: goto tr1596;
		case 72: goto tr1597;
		case 104: goto tr1597;
	}
	goto tr257;
tr1596:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1321;
st1321:
	if ( ++( p) == ( pe) )
		goto _test_eof1321;
case 1321:
#line 28108 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1599;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1321;
tr1599:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1322;
st1322:
	if ( ++( p) == ( pe) )
		goto _test_eof1322;
case 1322:
#line 28125 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 34: goto tr1599;
		case 62: goto st1323;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1321;
st1323:
	if ( ++( p) == ( pe) )
		goto _test_eof1323;
case 1323:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
	}
	goto tr1601;
tr1601:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1324;
st1324:
	if ( ++( p) == ( pe) )
		goto _test_eof1324;
case 1324:
#line 28153 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 60: goto tr1603;
	}
	goto st1324;
tr1603:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1325;
st1325:
	if ( ++( p) == ( pe) )
		goto _test_eof1325;
case 1325:
#line 28169 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 47: goto st1326;
		case 60: goto tr1603;
	}
	goto st1324;
st1326:
	if ( ++( p) == ( pe) )
		goto _test_eof1326;
case 1326:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 60: goto tr1603;
		case 65: goto st1327;
		case 97: goto st1327;
	}
	goto st1324;
st1327:
	if ( ++( p) == ( pe) )
		goto _test_eof1327;
case 1327:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 10: goto tr257;
		case 13: goto tr257;
		case 60: goto tr1603;
		case 62: goto tr1606;
	}
	goto st1324;
tr1597:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1328;
st1328:
	if ( ++( p) == ( pe) )
		goto _test_eof1328;
case 1328:
#line 28211 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1329;
		case 116: goto st1329;
	}
	goto tr257;
st1329:
	if ( ++( p) == ( pe) )
		goto _test_eof1329;
case 1329:
	switch( (*( p)) ) {
		case 84: goto st1330;
		case 116: goto st1330;
	}
	goto tr257;
st1330:
	if ( ++( p) == ( pe) )
		goto _test_eof1330;
case 1330:
	switch( (*( p)) ) {
		case 80: goto st1331;
		case 112: goto st1331;
	}
	goto tr257;
st1331:
	if ( ++( p) == ( pe) )
		goto _test_eof1331;
case 1331:
	switch( (*( p)) ) {
		case 58: goto st1332;
		case 83: goto st1335;
		case 115: goto st1335;
	}
	goto tr257;
st1332:
	if ( ++( p) == ( pe) )
		goto _test_eof1332;
case 1332:
	if ( (*( p)) == 47 )
		goto st1333;
	goto tr257;
st1333:
	if ( ++( p) == ( pe) )
		goto _test_eof1333;
case 1333:
	if ( (*( p)) == 47 )
		goto st1334;
	goto tr257;
st1334:
	if ( ++( p) == ( pe) )
		goto _test_eof1334;
case 1334:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1321;
st1335:
	if ( ++( p) == ( pe) )
		goto _test_eof1335;
case 1335:
	if ( (*( p)) == 58 )
		goto st1332;
	goto tr257;
st1336:
	if ( ++( p) == ( pe) )
		goto _test_eof1336;
case 1336:
	switch( (*( p)) ) {
		case 62: goto tr1078;
		case 76: goto st247;
		case 82: goto st1337;
		case 108: goto st247;
		case 114: goto st1337;
	}
	goto tr257;
st1337:
	if ( ++( p) == ( pe) )
		goto _test_eof1337;
case 1337:
	if ( (*( p)) == 62 )
		goto tr1079;
	goto tr257;
st1338:
	if ( ++( p) == ( pe) )
		goto _test_eof1338;
case 1338:
	switch( (*( p)) ) {
		case 79: goto st1339;
		case 111: goto st1339;
	}
	goto tr257;
st1339:
	if ( ++( p) == ( pe) )
		goto _test_eof1339;
case 1339:
	switch( (*( p)) ) {
		case 68: goto st1340;
		case 100: goto st1340;
	}
	goto tr257;
st1340:
	if ( ++( p) == ( pe) )
		goto _test_eof1340;
case 1340:
	switch( (*( p)) ) {
		case 69: goto st1341;
		case 101: goto st1341;
	}
	goto tr257;
st1341:
	if ( ++( p) == ( pe) )
		goto _test_eof1341;
case 1341:
	switch( (*( p)) ) {
		case 9: goto st1342;
		case 32: goto st1342;
		case 61: goto st1343;
		case 62: goto tr1086;
	}
	goto tr257;
st1342:
	if ( ++( p) == ( pe) )
		goto _test_eof1342;
case 1342:
	switch( (*( p)) ) {
		case 9: goto st1342;
		case 32: goto st1342;
		case 61: goto st1343;
	}
	goto tr257;
st1343:
	if ( ++( p) == ( pe) )
		goto _test_eof1343;
case 1343:
	switch( (*( p)) ) {
		case 9: goto st1343;
		case 32: goto st1343;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1620;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1620;
	} else
		goto tr1620;
	goto tr257;
tr1620:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1344;
st1344:
	if ( ++( p) == ( pe) )
		goto _test_eof1344;
case 1344:
#line 28369 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 62 )
		goto tr1622;
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1344;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1344;
	} else
		goto st1344;
	goto tr257;
tr1622:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1926;
st1926:
	if ( ++( p) == ( pe) )
		goto _test_eof1926;
case 1926:
#line 28391 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1091;
		case 9: goto st895;
		case 10: goto tr1091;
		case 32: goto st895;
	}
	goto tr2453;
st1345:
	if ( ++( p) == ( pe) )
		goto _test_eof1345;
case 1345:
	switch( (*( p)) ) {
		case 77: goto st1346;
		case 109: goto st1346;
	}
	goto tr257;
st1346:
	if ( ++( p) == ( pe) )
		goto _test_eof1346;
case 1346:
	if ( (*( p)) == 62 )
		goto tr1288;
	goto tr257;
tr2462:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1347;
st1347:
	if ( ++( p) == ( pe) )
		goto _test_eof1347;
case 1347:
#line 28423 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 84: goto st1348;
		case 116: goto st1348;
	}
	goto tr257;
st1348:
	if ( ++( p) == ( pe) )
		goto _test_eof1348;
case 1348:
	switch( (*( p)) ) {
		case 84: goto st1349;
		case 116: goto st1349;
	}
	goto tr257;
st1349:
	if ( ++( p) == ( pe) )
		goto _test_eof1349;
case 1349:
	switch( (*( p)) ) {
		case 80: goto st1350;
		case 112: goto st1350;
	}
	goto tr257;
st1350:
	if ( ++( p) == ( pe) )
		goto _test_eof1350;
case 1350:
	switch( (*( p)) ) {
		case 58: goto st1351;
		case 83: goto st1355;
		case 115: goto st1355;
	}
	goto tr257;
st1351:
	if ( ++( p) == ( pe) )
		goto _test_eof1351;
case 1351:
	if ( (*( p)) == 47 )
		goto st1352;
	goto tr257;
st1352:
	if ( ++( p) == ( pe) )
		goto _test_eof1352;
case 1352:
	if ( (*( p)) == 47 )
		goto st1353;
	goto tr257;
st1353:
	if ( ++( p) == ( pe) )
		goto _test_eof1353;
case 1353:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1354;
st1354:
	if ( ++( p) == ( pe) )
		goto _test_eof1354;
case 1354:
	switch( (*( p)) ) {
		case 0: goto tr257;
		case 32: goto tr257;
		case 62: goto tr1632;
	}
	if ( 9 <= (*( p)) && (*( p)) <= 13 )
		goto tr257;
	goto st1354;
st1355:
	if ( ++( p) == ( pe) )
		goto _test_eof1355;
case 1355:
	if ( (*( p)) == 58 )
		goto st1351;
	goto tr257;
st1356:
	if ( ++( p) == ( pe) )
		goto _test_eof1356;
case 1356:
	switch( (*( p)) ) {
		case 79: goto st1357;
		case 111: goto st1357;
	}
	goto tr257;
st1357:
	if ( ++( p) == ( pe) )
		goto _test_eof1357;
case 1357:
	switch( (*( p)) ) {
		case 68: goto st1358;
		case 100: goto st1358;
	}
	goto tr257;
st1358:
	if ( ++( p) == ( pe) )
		goto _test_eof1358;
case 1358:
	switch( (*( p)) ) {
		case 84: goto st1359;
		case 116: goto st1359;
	}
	goto tr257;
st1359:
	if ( ++( p) == ( pe) )
		goto _test_eof1359;
case 1359:
	switch( (*( p)) ) {
		case 69: goto st1360;
		case 101: goto st1360;
	}
	goto tr257;
st1360:
	if ( ++( p) == ( pe) )
		goto _test_eof1360;
case 1360:
	switch( (*( p)) ) {
		case 88: goto st1361;
		case 120: goto st1361;
	}
	goto tr257;
st1361:
	if ( ++( p) == ( pe) )
		goto _test_eof1361;
case 1361:
	switch( (*( p)) ) {
		case 84: goto st1362;
		case 116: goto st1362;
	}
	goto tr257;
st1362:
	if ( ++( p) == ( pe) )
		goto _test_eof1362;
case 1362:
	if ( (*( p)) == 62 )
		goto tr1295;
	goto tr257;
st1363:
	if ( ++( p) == ( pe) )
		goto _test_eof1363;
case 1363:
	switch( (*( p)) ) {
		case 62: goto tr1305;
		case 80: goto st1364;
		case 84: goto st1371;
		case 85: goto st1376;
		case 112: goto st1364;
		case 116: goto st1371;
		case 117: goto st1376;
	}
	goto tr257;
st1364:
	if ( ++( p) == ( pe) )
		goto _test_eof1364;
case 1364:
	switch( (*( p)) ) {
		case 79: goto st1365;
		case 111: goto st1365;
	}
	goto tr257;
st1365:
	if ( ++( p) == ( pe) )
		goto _test_eof1365;
case 1365:
	switch( (*( p)) ) {
		case 73: goto st1366;
		case 105: goto st1366;
	}
	goto tr257;
st1366:
	if ( ++( p) == ( pe) )
		goto _test_eof1366;
case 1366:
	switch( (*( p)) ) {
		case 76: goto st1367;
		case 108: goto st1367;
	}
	goto tr257;
st1367:
	if ( ++( p) == ( pe) )
		goto _test_eof1367;
case 1367:
	switch( (*( p)) ) {
		case 69: goto st1368;
		case 101: goto st1368;
	}
	goto tr257;
st1368:
	if ( ++( p) == ( pe) )
		goto _test_eof1368;
case 1368:
	switch( (*( p)) ) {
		case 82: goto st1369;
		case 114: goto st1369;
	}
	goto tr257;
st1369:
	if ( ++( p) == ( pe) )
		goto _test_eof1369;
case 1369:
	switch( (*( p)) ) {
		case 62: goto tr1312;
		case 83: goto st1370;
		case 115: goto st1370;
	}
	goto tr257;
st1370:
	if ( ++( p) == ( pe) )
		goto _test_eof1370;
case 1370:
	if ( (*( p)) == 62 )
		goto tr1312;
	goto tr257;
st1371:
	if ( ++( p) == ( pe) )
		goto _test_eof1371;
case 1371:
	switch( (*( p)) ) {
		case 82: goto st1372;
		case 114: goto st1372;
	}
	goto tr257;
st1372:
	if ( ++( p) == ( pe) )
		goto _test_eof1372;
case 1372:
	switch( (*( p)) ) {
		case 79: goto st1373;
		case 111: goto st1373;
	}
	goto tr257;
st1373:
	if ( ++( p) == ( pe) )
		goto _test_eof1373;
case 1373:
	switch( (*( p)) ) {
		case 78: goto st1374;
		case 110: goto st1374;
	}
	goto tr257;
st1374:
	if ( ++( p) == ( pe) )
		goto _test_eof1374;
case 1374:
	switch( (*( p)) ) {
		case 71: goto st1375;
		case 103: goto st1375;
	}
	goto tr257;
st1375:
	if ( ++( p) == ( pe) )
		goto _test_eof1375;
case 1375:
	if ( (*( p)) == 62 )
		goto tr1078;
	goto tr257;
st1376:
	if ( ++( p) == ( pe) )
		goto _test_eof1376;
case 1376:
	switch( (*( p)) ) {
		case 66: goto st1377;
		case 80: goto st1378;
		case 98: goto st1377;
		case 112: goto st1378;
	}
	goto tr257;
st1377:
	if ( ++( p) == ( pe) )
		goto _test_eof1377;
case 1377:
	if ( (*( p)) == 62 )
		goto tr1654;
	goto tr257;
st1378:
	if ( ++( p) == ( pe) )
		goto _test_eof1378;
case 1378:
	if ( (*( p)) == 62 )
		goto tr1655;
	goto tr257;
st1379:
	if ( ++( p) == ( pe) )
		goto _test_eof1379;
case 1379:
	switch( (*( p)) ) {
		case 78: goto st1380;
		case 110: goto st1380;
	}
	goto tr257;
st1380:
	if ( ++( p) == ( pe) )
		goto _test_eof1380;
case 1380:
	if ( (*( p)) == 62 )
		goto tr1326;
	goto tr257;
st1381:
	if ( ++( p) == ( pe) )
		goto _test_eof1381;
case 1381:
	if ( (*( p)) == 62 )
		goto tr1328;
	goto tr257;
tr2167:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1927;
st1927:
	if ( ++( p) == ( pe) )
		goto _test_eof1927;
case 1927:
#line 28741 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( 64 <= (*( p)) && (*( p)) <= 64 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 47: goto st1299;
		case 65: goto st1313;
		case 66: goto st1336;
		case 67: goto st1338;
		case 69: goto st1345;
		case 72: goto tr2462;
		case 73: goto st1346;
		case 78: goto st1356;
		case 81: goto st251;
		case 83: goto st1363;
		case 84: goto st1379;
		case 85: goto st1381;
		case 97: goto st1313;
		case 98: goto st1336;
		case 99: goto st1338;
		case 101: goto st1345;
		case 104: goto tr2462;
		case 105: goto st1346;
		case 110: goto st1356;
		case 113: goto st251;
		case 115: goto st1363;
		case 116: goto st1379;
		case 117: goto st1381;
		case 1088: goto st1382;
	}
	goto tr2169;
st1382:
	if ( ++( p) == ( pe) )
		goto _test_eof1382;
case 1382:
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
			goto tr1657;
	} else if ( _widec > 1032 ) {
		if ( _widec > 1055 ) {
			if ( 1057 <= _widec && _widec <= 1151 )
				goto tr1657;
		} else if ( _widec >= 1038 )
			goto tr1657;
	} else
		goto tr1657;
	goto tr257;
tr1658:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1383;
tr1657:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1383;
st1383:
	if ( ++( p) == ( pe) )
		goto _test_eof1383;
case 1383:
#line 28834 "ext/dtext/dtext.cpp"
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
		goto tr1659;
	if ( _widec < 1025 ) {
		if ( 896 <= _widec && _widec <= 1023 )
			goto tr1658;
	} else if ( _widec > 1033 ) {
		if ( _widec > 1036 ) {
			if ( 1038 <= _widec && _widec <= 1151 )
				goto tr1658;
		} else if ( _widec >= 1035 )
			goto tr1658;
	} else
		goto tr1658;
	goto tr257;
tr2168:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 629 "ext/dtext/dtext.cpp.rl"
	{( act) = 110;}
	goto st1928;
st1928:
	if ( ++( p) == ( pe) )
		goto _test_eof1928;
case 1928:
#line 28901 "ext/dtext/dtext.cpp"
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
		case 994: goto tr2470;
		case 995: goto tr2471;
		case 1007: goto tr2472;
		case 1070: goto tr2475;
		case 1119: goto tr2475;
		case 1151: goto tr2474;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( 962 <= _widec && _widec <= 991 )
				goto tr2468;
		} else if ( _widec > 1006 ) {
			if ( 1008 <= _widec && _widec <= 1012 )
				goto tr2473;
		} else
			goto tr2469;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( 1038 <= _widec && _widec <= 1055 )
				goto tr2474;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr2474;
			} else if ( _widec >= 1089 )
				goto tr2474;
		} else
			goto tr2474;
	} else
		goto tr2474;
	goto tr2169;
tr2468:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1384;
st1384:
	if ( ++( p) == ( pe) )
		goto _test_eof1384;
case 1384:
#line 29047 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1385;
	goto tr257;
tr2474:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1385;
st1385:
	if ( ++( p) == ( pe) )
		goto _test_eof1385;
case 1385:
#line 29066 "ext/dtext/dtext.cpp"
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
		case 994: goto st1399;
		case 995: goto st1401;
		case 1007: goto st1403;
		case 1057: goto st1385;
		case 1063: goto st1407;
		case 1067: goto st1385;
		case 1119: goto st1385;
		case 1151: goto tr1668;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1397;
			} else if ( _widec >= 896 )
				goto st1386;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1386;
			} else if ( _widec >= 1008 )
				goto st1406;
		} else
			goto st1398;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1385;
			} else if ( _widec >= 1038 )
				goto tr1668;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1668;
			} else if ( _widec >= 1089 )
				goto tr1668;
		} else
			goto tr1668;
	} else
		goto tr1668;
	goto tr250;
st1386:
	if ( ++( p) == ( pe) )
		goto _test_eof1386;
case 1386:
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1387;
			} else if ( _widec >= 896 )
				goto st1386;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1386;
			} else if ( _widec >= 1008 )
				goto st1395;
		} else
			goto st1388;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1386;
			} else if ( _widec >= 1038 )
				goto tr1676;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else if ( _widec >= 1089 )
				goto tr1676;
		} else
			goto tr1676;
	} else
		goto tr1676;
	goto tr250;
st1387:
	if ( ++( p) == ( pe) )
		goto _test_eof1387;
case 1387:
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto tr1676;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
tr1676:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 418 "ext/dtext/dtext.cpp.rl"
	{( act) = 58;}
	goto st1929;
st1929:
	if ( ++( p) == ( pe) )
		goto _test_eof1929;
case 1929:
#line 29646 "ext/dtext/dtext.cpp"
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1387;
			} else if ( _widec >= 896 )
				goto st1386;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1386;
			} else if ( _widec >= 1008 )
				goto st1395;
		} else
			goto st1388;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1386;
			} else if ( _widec >= 1038 )
				goto tr1676;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else if ( _widec >= 1089 )
				goto tr1676;
		} else
			goto tr1676;
	} else
		goto tr1676;
	goto tr2476;
st1388:
	if ( ++( p) == ( pe) )
		goto _test_eof1388;
case 1388:
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto st1387;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1389:
	if ( ++( p) == ( pe) )
		goto _test_eof1389;
case 1389:
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
		case 925: goto st1390;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto st1387;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1390:
	if ( ++( p) == ( pe) )
		goto _test_eof1390;
case 1390:
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
		case 941: goto st1386;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto tr1676;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1391:
	if ( ++( p) == ( pe) )
		goto _test_eof1391;
case 1391:
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
		case 896: goto st1392;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 897 )
				goto st1387;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1392:
	if ( ++( p) == ( pe) )
		goto _test_eof1392;
case 1392:
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 992 ) {
		if ( _widec < 914 ) {
			if ( _widec < 899 ) {
				if ( 896 <= _widec && _widec <= 898 )
					goto st1386;
			} else if ( _widec > 903 ) {
				if ( 904 <= _widec && _widec <= 913 )
					goto st1386;
			} else
				goto tr1676;
		} else if ( _widec > 915 ) {
			if ( _widec < 925 ) {
				if ( 916 <= _widec && _widec <= 924 )
					goto st1386;
			} else if ( _widec > 959 ) {
				if ( _widec > 961 ) {
					if ( 962 <= _widec && _widec <= 991 )
						goto st1387;
				} else if ( _widec >= 960 )
					goto st1386;
			} else
				goto tr1676;
		} else
			goto tr1676;
	} else if ( _widec > 1006 ) {
		if ( _widec < 1038 ) {
			if ( _widec < 1013 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec > 1023 ) {
				if ( 1025 <= _widec && _widec <= 1032 )
					goto tr1676;
			} else
				goto st1386;
		} else if ( _widec > 1055 ) {
			if ( _widec < 1072 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1386;
			} else if ( _widec > 1081 ) {
				if ( _widec > 1114 ) {
					if ( 1121 <= _widec && _widec <= 1146 )
						goto tr1676;
				} else if ( _widec >= 1089 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto tr1676;
	} else
		goto st1388;
	goto tr250;
st1393:
	if ( ++( p) == ( pe) )
		goto _test_eof1393;
case 1393:
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
		case 956: goto st1394;
		case 957: goto st1396;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto st1387;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1394:
	if ( ++( p) == ( pe) )
		goto _test_eof1394;
case 1394:
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
		case 905: goto st1386;
		case 957: goto st1386;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto tr1676;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto st1388;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1396:
	if ( ++( p) == ( pe) )
		goto _test_eof1396;
case 1396:
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
		case 925: goto st1386;
		case 928: goto st1386;
		case 931: goto st1386;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto tr1676;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1397:
	if ( ++( p) == ( pe) )
		goto _test_eof1397;
case 1397:
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto tr1668;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
tr1668:
#line 1 "NONE"
	{( te) = ( p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
#line 418 "ext/dtext/dtext.cpp.rl"
	{( act) = 58;}
	goto st1930;
st1930:
	if ( ++( p) == ( pe) )
		goto _test_eof1930;
case 1930:
#line 31998 "ext/dtext/dtext.cpp"
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
		case 994: goto st1399;
		case 995: goto st1401;
		case 1007: goto st1403;
		case 1057: goto st1385;
		case 1063: goto st1407;
		case 1067: goto st1385;
		case 1119: goto st1385;
		case 1151: goto tr1668;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1397;
			} else if ( _widec >= 896 )
				goto st1386;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1386;
			} else if ( _widec >= 1008 )
				goto st1406;
		} else
			goto st1398;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1385;
			} else if ( _widec >= 1038 )
				goto tr1668;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1668;
			} else if ( _widec >= 1089 )
				goto tr1668;
		} else
			goto tr1668;
	} else
		goto tr1668;
	goto tr2476;
st1398:
	if ( ++( p) == ( pe) )
		goto _test_eof1398;
case 1398:
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto st1397;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1399:
	if ( ++( p) == ( pe) )
		goto _test_eof1399;
case 1399:
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
		case 925: goto st1400;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto st1397;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1400:
	if ( ++( p) == ( pe) )
		goto _test_eof1400;
case 1400:
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
		case 941: goto st1385;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto tr1668;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1401:
	if ( ++( p) == ( pe) )
		goto _test_eof1401;
case 1401:
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
		case 896: goto st1402;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 897 )
				goto st1397;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1402:
	if ( ++( p) == ( pe) )
		goto _test_eof1402;
case 1402:
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 992 ) {
		if ( _widec < 914 ) {
			if ( _widec < 899 ) {
				if ( 896 <= _widec && _widec <= 898 )
					goto st1385;
			} else if ( _widec > 903 ) {
				if ( 904 <= _widec && _widec <= 913 )
					goto st1385;
			} else
				goto tr1668;
		} else if ( _widec > 915 ) {
			if ( _widec < 925 ) {
				if ( 916 <= _widec && _widec <= 924 )
					goto st1385;
			} else if ( _widec > 959 ) {
				if ( _widec > 961 ) {
					if ( 962 <= _widec && _widec <= 991 )
						goto st1387;
				} else if ( _widec >= 960 )
					goto st1386;
			} else
				goto tr1668;
		} else
			goto tr1668;
	} else if ( _widec > 1006 ) {
		if ( _widec < 1038 ) {
			if ( _widec < 1013 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec > 1023 ) {
				if ( 1025 <= _widec && _widec <= 1032 )
					goto tr1676;
			} else
				goto st1386;
		} else if ( _widec > 1055 ) {
			if ( _widec < 1072 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1386;
			} else if ( _widec > 1081 ) {
				if ( _widec > 1114 ) {
					if ( 1121 <= _widec && _widec <= 1146 )
						goto tr1676;
				} else if ( _widec >= 1089 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto tr1676;
	} else
		goto st1388;
	goto tr250;
st1403:
	if ( ++( p) == ( pe) )
		goto _test_eof1403;
case 1403:
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
		case 956: goto st1404;
		case 957: goto st1405;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto st1397;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1404:
	if ( ++( p) == ( pe) )
		goto _test_eof1404;
case 1404:
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
		case 905: goto st1385;
		case 957: goto st1385;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto tr1668;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1405:
	if ( ++( p) == ( pe) )
		goto _test_eof1405;
case 1405:
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
		case 925: goto st1385;
		case 928: goto st1385;
		case 931: goto st1385;
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto tr1668;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1406:
	if ( ++( p) == ( pe) )
		goto _test_eof1406;
case 1406:
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
		case 994: goto st1389;
		case 995: goto st1391;
		case 1007: goto st1393;
		case 1057: goto st1386;
		case 1063: goto st1386;
		case 1067: goto st1386;
		case 1119: goto st1386;
		case 1151: goto tr1676;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1386;
			} else if ( _widec >= 896 )
				goto st1398;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1395;
			} else if ( _widec >= 992 )
				goto st1388;
		} else
			goto st1387;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1676;
			} else if ( _widec >= 1025 )
				goto tr1676;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1676;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1676;
			} else
				goto tr1676;
		} else
			goto st1386;
	} else
		goto st1386;
	goto tr250;
st1407:
	if ( ++( p) == ( pe) )
		goto _test_eof1407;
case 1407:
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
		case 994: goto st1399;
		case 995: goto st1401;
		case 1007: goto st1403;
		case 1057: goto st1385;
		case 1063: goto st1407;
		case 1067: goto st1385;
		case 1119: goto st1385;
		case 1124: goto st1385;
		case 1139: goto st1385;
		case 1151: goto tr1668;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1397;
			} else if ( _widec >= 896 )
				goto st1386;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1386;
			} else if ( _widec >= 1008 )
				goto st1406;
		} else
			goto st1398;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1385;
			} else if ( _widec >= 1038 )
				goto tr1668;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1668;
			} else if ( _widec >= 1089 )
				goto tr1668;
		} else
			goto tr1668;
	} else
		goto tr1668;
	goto tr250;
tr2469:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1408;
st1408:
	if ( ++( p) == ( pe) )
		goto _test_eof1408;
case 1408:
#line 34366 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1384;
	goto tr257;
tr2470:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1409;
st1409:
	if ( ++( p) == ( pe) )
		goto _test_eof1409;
case 1409:
#line 34385 "ext/dtext/dtext.cpp"
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
		goto st1410;
	if ( 896 <= _widec && _widec <= 959 )
		goto st1384;
	goto tr257;
st1410:
	if ( ++( p) == ( pe) )
		goto _test_eof1410;
case 1410:
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
			goto st1385;
	} else if ( _widec >= 896 )
		goto st1385;
	goto tr257;
tr2471:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1411;
st1411:
	if ( ++( p) == ( pe) )
		goto _test_eof1411;
case 1411:
#line 34444 "ext/dtext/dtext.cpp"
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
		goto st1412;
	if ( 897 <= _widec && _widec <= 959 )
		goto st1384;
	goto tr257;
st1412:
	if ( ++( p) == ( pe) )
		goto _test_eof1412;
case 1412:
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
			goto st1385;
	} else if ( _widec > 915 ) {
		if ( 925 <= _widec && _widec <= 959 )
			goto st1385;
	} else
		goto st1385;
	goto tr257;
tr2472:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1413;
st1413:
	if ( ++( p) == ( pe) )
		goto _test_eof1413;
case 1413:
#line 34506 "ext/dtext/dtext.cpp"
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
		case 956: goto st1414;
		case 957: goto st1415;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1384;
	goto tr257;
st1414:
	if ( ++( p) == ( pe) )
		goto _test_eof1414;
case 1414:
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
			goto st1385;
	} else if ( _widec > 956 ) {
		if ( 958 <= _widec && _widec <= 959 )
			goto st1385;
	} else
		goto st1385;
	goto tr257;
st1415:
	if ( ++( p) == ( pe) )
		goto _test_eof1415;
case 1415:
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
			goto st1385;
	} else if ( _widec > 927 ) {
		if ( _widec > 930 ) {
			if ( 932 <= _widec && _widec <= 959 )
				goto st1385;
		} else if ( _widec >= 929 )
			goto st1385;
	} else
		goto st1385;
	goto tr257;
tr2473:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1416;
st1416:
	if ( ++( p) == ( pe) )
		goto _test_eof1416;
case 1416:
#line 34628 "ext/dtext/dtext.cpp"
	_widec = (*( p));
	if ( (*( p)) <= -65 ) {
		_widec = (short)(640 + ((*( p)) - -128));
		if ( 
#line 89 "ext/dtext/dtext.cpp.rl"
 options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1408;
	goto tr257;
tr2475:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1417;
st1417:
	if ( ++( p) == ( pe) )
		goto _test_eof1417;
case 1417:
#line 34647 "ext/dtext/dtext.cpp"
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
		case 994: goto st1409;
		case 995: goto st1411;
		case 1007: goto st1413;
		case 1151: goto st1385;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( 962 <= _widec && _widec <= 991 )
				goto st1384;
		} else if ( _widec > 1006 ) {
			if ( 1008 <= _widec && _widec <= 1012 )
				goto st1416;
		} else
			goto st1408;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( 1038 <= _widec && _widec <= 1055 )
				goto st1385;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto st1385;
			} else if ( _widec >= 1089 )
				goto st1385;
		} else
			goto st1385;
	} else
		goto st1385;
	goto tr257;
tr1695:
#line 642 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1931;
tr1701:
#line 635 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_rewind();
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1931;
tr2477:
#line 642 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1931;
tr2478:
#line 640 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1931;
tr2482:
#line 642 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1931;
st1931:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1931;
case 1931:
#line 1 "NONE"
	{( ts) = ( p);}
#line 34806 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2478;
		case 10: goto tr2479;
		case 60: goto tr2480;
		case 91: goto tr2481;
	}
	goto tr2477;
tr2479:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1932;
st1932:
	if ( ++( p) == ( pe) )
		goto _test_eof1932;
case 1932:
#line 34822 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto st1418;
		case 91: goto st1424;
	}
	goto tr2482;
st1418:
	if ( ++( p) == ( pe) )
		goto _test_eof1418;
case 1418:
	if ( (*( p)) == 47 )
		goto st1419;
	goto tr1695;
st1419:
	if ( ++( p) == ( pe) )
		goto _test_eof1419;
case 1419:
	switch( (*( p)) ) {
		case 67: goto st1420;
		case 99: goto st1420;
	}
	goto tr1695;
st1420:
	if ( ++( p) == ( pe) )
		goto _test_eof1420;
case 1420:
	switch( (*( p)) ) {
		case 79: goto st1421;
		case 111: goto st1421;
	}
	goto tr1695;
st1421:
	if ( ++( p) == ( pe) )
		goto _test_eof1421;
case 1421:
	switch( (*( p)) ) {
		case 68: goto st1422;
		case 100: goto st1422;
	}
	goto tr1695;
st1422:
	if ( ++( p) == ( pe) )
		goto _test_eof1422;
case 1422:
	switch( (*( p)) ) {
		case 69: goto st1423;
		case 101: goto st1423;
	}
	goto tr1695;
st1423:
	if ( ++( p) == ( pe) )
		goto _test_eof1423;
case 1423:
	if ( (*( p)) == 62 )
		goto tr1701;
	goto tr1695;
st1424:
	if ( ++( p) == ( pe) )
		goto _test_eof1424;
case 1424:
	if ( (*( p)) == 47 )
		goto st1425;
	goto tr1695;
st1425:
	if ( ++( p) == ( pe) )
		goto _test_eof1425;
case 1425:
	switch( (*( p)) ) {
		case 67: goto st1426;
		case 99: goto st1426;
	}
	goto tr1695;
st1426:
	if ( ++( p) == ( pe) )
		goto _test_eof1426;
case 1426:
	switch( (*( p)) ) {
		case 79: goto st1427;
		case 111: goto st1427;
	}
	goto tr1695;
st1427:
	if ( ++( p) == ( pe) )
		goto _test_eof1427;
case 1427:
	switch( (*( p)) ) {
		case 68: goto st1428;
		case 100: goto st1428;
	}
	goto tr1695;
st1428:
	if ( ++( p) == ( pe) )
		goto _test_eof1428;
case 1428:
	switch( (*( p)) ) {
		case 69: goto st1429;
		case 101: goto st1429;
	}
	goto tr1695;
st1429:
	if ( ++( p) == ( pe) )
		goto _test_eof1429;
case 1429:
	if ( (*( p)) == 93 )
		goto tr1701;
	goto tr1695;
tr2480:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1933;
st1933:
	if ( ++( p) == ( pe) )
		goto _test_eof1933;
case 1933:
#line 34936 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1419;
	goto tr2482;
tr2481:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1934;
st1934:
	if ( ++( p) == ( pe) )
		goto _test_eof1934;
case 1934:
#line 34948 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1425;
	goto tr2482;
tr1707:
#line 655 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}{
    append_html_escaped((*( p)));
  }}
	goto st1935;
tr1716:
#line 648 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_rewind();
    {( cs) = ( (stack.data()))[--( top)];goto _again;}
  }}
	goto st1935;
tr2485:
#line 655 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    append_html_escaped((*( p)));
  }}
	goto st1935;
tr2486:
#line 653 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1935;
tr2490:
#line 655 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;{
    append_html_escaped((*( p)));
  }}
	goto st1935;
st1935:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1935;
case 1935:
#line 1 "NONE"
	{( ts) = ( p);}
#line 34989 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr2486;
		case 10: goto tr2487;
		case 60: goto tr2488;
		case 91: goto tr2489;
	}
	goto tr2485;
tr2487:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1936;
st1936:
	if ( ++( p) == ( pe) )
		goto _test_eof1936;
case 1936:
#line 35005 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto st1430;
		case 91: goto st1439;
	}
	goto tr2490;
st1430:
	if ( ++( p) == ( pe) )
		goto _test_eof1430;
case 1430:
	if ( (*( p)) == 47 )
		goto st1431;
	goto tr1707;
st1431:
	if ( ++( p) == ( pe) )
		goto _test_eof1431;
case 1431:
	switch( (*( p)) ) {
		case 78: goto st1432;
		case 110: goto st1432;
	}
	goto tr1707;
st1432:
	if ( ++( p) == ( pe) )
		goto _test_eof1432;
case 1432:
	switch( (*( p)) ) {
		case 79: goto st1433;
		case 111: goto st1433;
	}
	goto tr1707;
st1433:
	if ( ++( p) == ( pe) )
		goto _test_eof1433;
case 1433:
	switch( (*( p)) ) {
		case 68: goto st1434;
		case 100: goto st1434;
	}
	goto tr1707;
st1434:
	if ( ++( p) == ( pe) )
		goto _test_eof1434;
case 1434:
	switch( (*( p)) ) {
		case 84: goto st1435;
		case 116: goto st1435;
	}
	goto tr1707;
st1435:
	if ( ++( p) == ( pe) )
		goto _test_eof1435;
case 1435:
	switch( (*( p)) ) {
		case 69: goto st1436;
		case 101: goto st1436;
	}
	goto tr1707;
st1436:
	if ( ++( p) == ( pe) )
		goto _test_eof1436;
case 1436:
	switch( (*( p)) ) {
		case 88: goto st1437;
		case 120: goto st1437;
	}
	goto tr1707;
st1437:
	if ( ++( p) == ( pe) )
		goto _test_eof1437;
case 1437:
	switch( (*( p)) ) {
		case 84: goto st1438;
		case 116: goto st1438;
	}
	goto tr1707;
st1438:
	if ( ++( p) == ( pe) )
		goto _test_eof1438;
case 1438:
	if ( (*( p)) == 62 )
		goto tr1716;
	goto tr1707;
st1439:
	if ( ++( p) == ( pe) )
		goto _test_eof1439;
case 1439:
	if ( (*( p)) == 47 )
		goto st1440;
	goto tr1707;
st1440:
	if ( ++( p) == ( pe) )
		goto _test_eof1440;
case 1440:
	switch( (*( p)) ) {
		case 78: goto st1441;
		case 110: goto st1441;
	}
	goto tr1707;
st1441:
	if ( ++( p) == ( pe) )
		goto _test_eof1441;
case 1441:
	switch( (*( p)) ) {
		case 79: goto st1442;
		case 111: goto st1442;
	}
	goto tr1707;
st1442:
	if ( ++( p) == ( pe) )
		goto _test_eof1442;
case 1442:
	switch( (*( p)) ) {
		case 68: goto st1443;
		case 100: goto st1443;
	}
	goto tr1707;
st1443:
	if ( ++( p) == ( pe) )
		goto _test_eof1443;
case 1443:
	switch( (*( p)) ) {
		case 84: goto st1444;
		case 116: goto st1444;
	}
	goto tr1707;
st1444:
	if ( ++( p) == ( pe) )
		goto _test_eof1444;
case 1444:
	switch( (*( p)) ) {
		case 69: goto st1445;
		case 101: goto st1445;
	}
	goto tr1707;
st1445:
	if ( ++( p) == ( pe) )
		goto _test_eof1445;
case 1445:
	switch( (*( p)) ) {
		case 88: goto st1446;
		case 120: goto st1446;
	}
	goto tr1707;
st1446:
	if ( ++( p) == ( pe) )
		goto _test_eof1446;
case 1446:
	switch( (*( p)) ) {
		case 84: goto st1447;
		case 116: goto st1447;
	}
	goto tr1707;
st1447:
	if ( ++( p) == ( pe) )
		goto _test_eof1447;
case 1447:
	if ( (*( p)) == 93 )
		goto tr1716;
	goto tr1707;
tr2488:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1937;
st1937:
	if ( ++( p) == ( pe) )
		goto _test_eof1937;
case 1937:
#line 35173 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1431;
	goto tr2490;
tr2489:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1938;
st1938:
	if ( ++( p) == ( pe) )
		goto _test_eof1938;
case 1938:
#line 35185 "ext/dtext/dtext.cpp"
	if ( (*( p)) == 47 )
		goto st1440;
	goto tr2490;
tr1725:
#line 714 "ext/dtext/dtext.cpp.rl"
	{{( p) = ((( te)))-1;}}
	goto st1939;
tr1735:
#line 665 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_COLGROUP, { ts, te });
  }}
	goto st1939;
tr1743:
#line 708 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    if (dstack_close_element(BLOCK_TABLE, { ts, te })) {
      {( cs) = ( (stack.data()))[--( top)];goto _again;}
    }
  }}
	goto st1939;
tr1747:
#line 686 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_TBODY, { ts, te });
  }}
	goto st1939;
tr1751:
#line 678 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_THEAD, { ts, te });
  }}
	goto st1939;
tr1752:
#line 699 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_close_element(BLOCK_TR, { ts, te });
  }}
	goto st1939;
tr1756:
#line 669 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1939;
tr1771:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 669 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1939;
tr1776:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 669 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COL, "col");
    dstack_rewind();
  }}
	goto st1939;
tr1782:
#line 661 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1939;
tr1796:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 661 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1939;
tr1801:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 661 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_COLGROUP, "colgroup");
  }}
	goto st1939;
tr1810:
#line 682 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1939;
tr1824:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 682 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1939;
tr1829:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 682 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TBODY, "tbody");
  }}
	goto st1939;
tr1831:
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
{( (stack.data()))[( top)++] = 1939;goto st1701;}}
  }}
	goto st1939;
tr1845:
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
{( (stack.data()))[( top)++] = 1939;goto st1701;}}
  }}
	goto st1939;
tr1850:
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
{( (stack.data()))[( top)++] = 1939;goto st1701;}}
  }}
	goto st1939;
tr1852:
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
{( (stack.data()))[( top)++] = 1939;goto st1701;}}
  }}
	goto st1939;
tr1867:
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
{( (stack.data()))[( top)++] = 1939;goto st1701;}}
  }}
	goto st1939;
tr1872:
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
{( (stack.data()))[( top)++] = 1939;goto st1701;}}
  }}
	goto st1939;
tr1876:
#line 674 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1939;
tr1890:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 674 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1939;
tr1895:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 674 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_THEAD, "thead");
  }}
	goto st1939;
tr1897:
#line 695 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1939;
tr1911:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 695 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1939;
tr1916:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
#line 695 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;{
    dstack_open_element_attributes(BLOCK_TR, "tr");
  }}
	goto st1939;
tr2493:
#line 714 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p)+1;}
	goto st1939;
tr2496:
#line 714 "ext/dtext/dtext.cpp.rl"
	{( te) = ( p);( p)--;}
	goto st1939;
st1939:
#line 1 "NONE"
	{( ts) = 0;}
	if ( ++( p) == ( pe) )
		goto _test_eof1939;
case 1939:
#line 1 "NONE"
	{( ts) = ( p);}
#line 35490 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 60: goto tr2494;
		case 91: goto tr2495;
	}
	goto tr2493;
tr2494:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1940;
st1940:
	if ( ++( p) == ( pe) )
		goto _test_eof1940;
case 1940:
#line 35504 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1448;
		case 67: goto st1471;
		case 84: goto st1499;
		case 99: goto st1471;
		case 116: goto st1499;
	}
	goto tr2496;
st1448:
	if ( ++( p) == ( pe) )
		goto _test_eof1448;
case 1448:
	switch( (*( p)) ) {
		case 67: goto st1449;
		case 84: goto st1457;
		case 99: goto st1449;
		case 116: goto st1457;
	}
	goto tr1725;
st1449:
	if ( ++( p) == ( pe) )
		goto _test_eof1449;
case 1449:
	switch( (*( p)) ) {
		case 79: goto st1450;
		case 111: goto st1450;
	}
	goto tr1725;
st1450:
	if ( ++( p) == ( pe) )
		goto _test_eof1450;
case 1450:
	switch( (*( p)) ) {
		case 76: goto st1451;
		case 108: goto st1451;
	}
	goto tr1725;
st1451:
	if ( ++( p) == ( pe) )
		goto _test_eof1451;
case 1451:
	switch( (*( p)) ) {
		case 71: goto st1452;
		case 103: goto st1452;
	}
	goto tr1725;
st1452:
	if ( ++( p) == ( pe) )
		goto _test_eof1452;
case 1452:
	switch( (*( p)) ) {
		case 82: goto st1453;
		case 114: goto st1453;
	}
	goto tr1725;
st1453:
	if ( ++( p) == ( pe) )
		goto _test_eof1453;
case 1453:
	switch( (*( p)) ) {
		case 79: goto st1454;
		case 111: goto st1454;
	}
	goto tr1725;
st1454:
	if ( ++( p) == ( pe) )
		goto _test_eof1454;
case 1454:
	switch( (*( p)) ) {
		case 85: goto st1455;
		case 117: goto st1455;
	}
	goto tr1725;
st1455:
	if ( ++( p) == ( pe) )
		goto _test_eof1455;
case 1455:
	switch( (*( p)) ) {
		case 80: goto st1456;
		case 112: goto st1456;
	}
	goto tr1725;
st1456:
	if ( ++( p) == ( pe) )
		goto _test_eof1456;
case 1456:
	if ( (*( p)) == 62 )
		goto tr1735;
	goto tr1725;
st1457:
	if ( ++( p) == ( pe) )
		goto _test_eof1457;
case 1457:
	switch( (*( p)) ) {
		case 65: goto st1458;
		case 66: goto st1462;
		case 72: goto st1466;
		case 82: goto st1470;
		case 97: goto st1458;
		case 98: goto st1462;
		case 104: goto st1466;
		case 114: goto st1470;
	}
	goto tr1725;
st1458:
	if ( ++( p) == ( pe) )
		goto _test_eof1458;
case 1458:
	switch( (*( p)) ) {
		case 66: goto st1459;
		case 98: goto st1459;
	}
	goto tr1725;
st1459:
	if ( ++( p) == ( pe) )
		goto _test_eof1459;
case 1459:
	switch( (*( p)) ) {
		case 76: goto st1460;
		case 108: goto st1460;
	}
	goto tr1725;
st1460:
	if ( ++( p) == ( pe) )
		goto _test_eof1460;
case 1460:
	switch( (*( p)) ) {
		case 69: goto st1461;
		case 101: goto st1461;
	}
	goto tr1725;
st1461:
	if ( ++( p) == ( pe) )
		goto _test_eof1461;
case 1461:
	if ( (*( p)) == 62 )
		goto tr1743;
	goto tr1725;
st1462:
	if ( ++( p) == ( pe) )
		goto _test_eof1462;
case 1462:
	switch( (*( p)) ) {
		case 79: goto st1463;
		case 111: goto st1463;
	}
	goto tr1725;
st1463:
	if ( ++( p) == ( pe) )
		goto _test_eof1463;
case 1463:
	switch( (*( p)) ) {
		case 68: goto st1464;
		case 100: goto st1464;
	}
	goto tr1725;
st1464:
	if ( ++( p) == ( pe) )
		goto _test_eof1464;
case 1464:
	switch( (*( p)) ) {
		case 89: goto st1465;
		case 121: goto st1465;
	}
	goto tr1725;
st1465:
	if ( ++( p) == ( pe) )
		goto _test_eof1465;
case 1465:
	if ( (*( p)) == 62 )
		goto tr1747;
	goto tr1725;
st1466:
	if ( ++( p) == ( pe) )
		goto _test_eof1466;
case 1466:
	switch( (*( p)) ) {
		case 69: goto st1467;
		case 101: goto st1467;
	}
	goto tr1725;
st1467:
	if ( ++( p) == ( pe) )
		goto _test_eof1467;
case 1467:
	switch( (*( p)) ) {
		case 65: goto st1468;
		case 97: goto st1468;
	}
	goto tr1725;
st1468:
	if ( ++( p) == ( pe) )
		goto _test_eof1468;
case 1468:
	switch( (*( p)) ) {
		case 68: goto st1469;
		case 100: goto st1469;
	}
	goto tr1725;
st1469:
	if ( ++( p) == ( pe) )
		goto _test_eof1469;
case 1469:
	if ( (*( p)) == 62 )
		goto tr1751;
	goto tr1725;
st1470:
	if ( ++( p) == ( pe) )
		goto _test_eof1470;
case 1470:
	if ( (*( p)) == 62 )
		goto tr1752;
	goto tr1725;
st1471:
	if ( ++( p) == ( pe) )
		goto _test_eof1471;
case 1471:
	switch( (*( p)) ) {
		case 79: goto st1472;
		case 111: goto st1472;
	}
	goto tr1725;
st1472:
	if ( ++( p) == ( pe) )
		goto _test_eof1472;
case 1472:
	switch( (*( p)) ) {
		case 76: goto st1473;
		case 108: goto st1473;
	}
	goto tr1725;
st1473:
	if ( ++( p) == ( pe) )
		goto _test_eof1473;
case 1473:
	switch( (*( p)) ) {
		case 9: goto st1474;
		case 32: goto st1474;
		case 62: goto tr1756;
		case 71: goto st1484;
		case 103: goto st1484;
	}
	goto tr1725;
tr1770:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1474;
tr1774:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1474;
st1474:
	if ( ++( p) == ( pe) )
		goto _test_eof1474;
case 1474:
#line 35762 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1474;
		case 32: goto st1474;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1758;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1758;
	} else
		goto tr1758;
	goto tr1725;
tr1758:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1475;
st1475:
	if ( ++( p) == ( pe) )
		goto _test_eof1475;
case 1475:
#line 35784 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1759;
		case 32: goto tr1759;
		case 61: goto tr1761;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1475;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1475;
	} else
		goto st1475;
	goto tr1725;
tr1759:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1476;
st1476:
	if ( ++( p) == ( pe) )
		goto _test_eof1476;
case 1476:
#line 35807 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1476;
		case 32: goto st1476;
		case 61: goto st1477;
	}
	goto tr1725;
tr1761:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1477;
st1477:
	if ( ++( p) == ( pe) )
		goto _test_eof1477;
case 1477:
#line 35822 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1477;
		case 32: goto st1477;
		case 34: goto st1478;
		case 39: goto st1481;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1766;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1766;
	} else
		goto tr1766;
	goto tr1725;
st1478:
	if ( ++( p) == ( pe) )
		goto _test_eof1478;
case 1478:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1767;
tr1767:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1479;
st1479:
	if ( ++( p) == ( pe) )
		goto _test_eof1479;
case 1479:
#line 35856 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1769;
	}
	goto st1479;
tr1769:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1480;
st1480:
	if ( ++( p) == ( pe) )
		goto _test_eof1480;
case 1480:
#line 35872 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1770;
		case 32: goto tr1770;
		case 62: goto tr1771;
	}
	goto tr1725;
st1481:
	if ( ++( p) == ( pe) )
		goto _test_eof1481;
case 1481:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1772;
tr1772:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1482;
st1482:
	if ( ++( p) == ( pe) )
		goto _test_eof1482;
case 1482:
#line 35897 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1769;
	}
	goto st1482;
tr1766:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1483;
st1483:
	if ( ++( p) == ( pe) )
		goto _test_eof1483;
case 1483:
#line 35913 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1774;
		case 32: goto tr1774;
		case 62: goto tr1776;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1483;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1483;
	} else
		goto st1483;
	goto tr1725;
st1484:
	if ( ++( p) == ( pe) )
		goto _test_eof1484;
case 1484:
	switch( (*( p)) ) {
		case 82: goto st1485;
		case 114: goto st1485;
	}
	goto tr1725;
st1485:
	if ( ++( p) == ( pe) )
		goto _test_eof1485;
case 1485:
	switch( (*( p)) ) {
		case 79: goto st1486;
		case 111: goto st1486;
	}
	goto tr1725;
st1486:
	if ( ++( p) == ( pe) )
		goto _test_eof1486;
case 1486:
	switch( (*( p)) ) {
		case 85: goto st1487;
		case 117: goto st1487;
	}
	goto tr1725;
st1487:
	if ( ++( p) == ( pe) )
		goto _test_eof1487;
case 1487:
	switch( (*( p)) ) {
		case 80: goto st1488;
		case 112: goto st1488;
	}
	goto tr1725;
st1488:
	if ( ++( p) == ( pe) )
		goto _test_eof1488;
case 1488:
	switch( (*( p)) ) {
		case 9: goto st1489;
		case 32: goto st1489;
		case 62: goto tr1782;
	}
	goto tr1725;
tr1795:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1489;
tr1799:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1489;
st1489:
	if ( ++( p) == ( pe) )
		goto _test_eof1489;
case 1489:
#line 35988 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1489;
		case 32: goto st1489;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1783;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1783;
	} else
		goto tr1783;
	goto tr1725;
tr1783:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1490;
st1490:
	if ( ++( p) == ( pe) )
		goto _test_eof1490;
case 1490:
#line 36010 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1784;
		case 32: goto tr1784;
		case 61: goto tr1786;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1490;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1490;
	} else
		goto st1490;
	goto tr1725;
tr1784:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1491;
st1491:
	if ( ++( p) == ( pe) )
		goto _test_eof1491;
case 1491:
#line 36033 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1491;
		case 32: goto st1491;
		case 61: goto st1492;
	}
	goto tr1725;
tr1786:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1492;
st1492:
	if ( ++( p) == ( pe) )
		goto _test_eof1492;
case 1492:
#line 36048 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1492;
		case 32: goto st1492;
		case 34: goto st1493;
		case 39: goto st1496;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1791;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1791;
	} else
		goto tr1791;
	goto tr1725;
st1493:
	if ( ++( p) == ( pe) )
		goto _test_eof1493;
case 1493:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1792;
tr1792:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1494;
st1494:
	if ( ++( p) == ( pe) )
		goto _test_eof1494;
case 1494:
#line 36082 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1794;
	}
	goto st1494;
tr1794:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1495;
st1495:
	if ( ++( p) == ( pe) )
		goto _test_eof1495;
case 1495:
#line 36098 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1795;
		case 32: goto tr1795;
		case 62: goto tr1796;
	}
	goto tr1725;
st1496:
	if ( ++( p) == ( pe) )
		goto _test_eof1496;
case 1496:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1797;
tr1797:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1497;
st1497:
	if ( ++( p) == ( pe) )
		goto _test_eof1497;
case 1497:
#line 36123 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1794;
	}
	goto st1497;
tr1791:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1498;
st1498:
	if ( ++( p) == ( pe) )
		goto _test_eof1498;
case 1498:
#line 36139 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1799;
		case 32: goto tr1799;
		case 62: goto tr1801;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1498;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1498;
	} else
		goto st1498;
	goto tr1725;
st1499:
	if ( ++( p) == ( pe) )
		goto _test_eof1499;
case 1499:
	switch( (*( p)) ) {
		case 66: goto st1500;
		case 68: goto st1514;
		case 72: goto st1525;
		case 82: goto st1549;
		case 98: goto st1500;
		case 100: goto st1514;
		case 104: goto st1525;
		case 114: goto st1549;
	}
	goto tr1725;
st1500:
	if ( ++( p) == ( pe) )
		goto _test_eof1500;
case 1500:
	switch( (*( p)) ) {
		case 79: goto st1501;
		case 111: goto st1501;
	}
	goto tr1725;
st1501:
	if ( ++( p) == ( pe) )
		goto _test_eof1501;
case 1501:
	switch( (*( p)) ) {
		case 68: goto st1502;
		case 100: goto st1502;
	}
	goto tr1725;
st1502:
	if ( ++( p) == ( pe) )
		goto _test_eof1502;
case 1502:
	switch( (*( p)) ) {
		case 89: goto st1503;
		case 121: goto st1503;
	}
	goto tr1725;
st1503:
	if ( ++( p) == ( pe) )
		goto _test_eof1503;
case 1503:
	switch( (*( p)) ) {
		case 9: goto st1504;
		case 32: goto st1504;
		case 62: goto tr1810;
	}
	goto tr1725;
tr1823:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1504;
tr1827:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1504;
st1504:
	if ( ++( p) == ( pe) )
		goto _test_eof1504;
case 1504:
#line 36220 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1504;
		case 32: goto st1504;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1811;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1811;
	} else
		goto tr1811;
	goto tr1725;
tr1811:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1505;
st1505:
	if ( ++( p) == ( pe) )
		goto _test_eof1505;
case 1505:
#line 36242 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1812;
		case 32: goto tr1812;
		case 61: goto tr1814;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1505;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1505;
	} else
		goto st1505;
	goto tr1725;
tr1812:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1506;
st1506:
	if ( ++( p) == ( pe) )
		goto _test_eof1506;
case 1506:
#line 36265 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1506;
		case 32: goto st1506;
		case 61: goto st1507;
	}
	goto tr1725;
tr1814:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1507;
st1507:
	if ( ++( p) == ( pe) )
		goto _test_eof1507;
case 1507:
#line 36280 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1507;
		case 32: goto st1507;
		case 34: goto st1508;
		case 39: goto st1511;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1819;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1725;
st1508:
	if ( ++( p) == ( pe) )
		goto _test_eof1508;
case 1508:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1820;
tr1820:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1509;
st1509:
	if ( ++( p) == ( pe) )
		goto _test_eof1509;
case 1509:
#line 36314 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1822;
	}
	goto st1509;
tr1822:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1510;
st1510:
	if ( ++( p) == ( pe) )
		goto _test_eof1510;
case 1510:
#line 36330 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1823;
		case 32: goto tr1823;
		case 62: goto tr1824;
	}
	goto tr1725;
st1511:
	if ( ++( p) == ( pe) )
		goto _test_eof1511;
case 1511:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1825;
tr1825:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1512;
st1512:
	if ( ++( p) == ( pe) )
		goto _test_eof1512;
case 1512:
#line 36355 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1822;
	}
	goto st1512;
tr1819:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1513;
st1513:
	if ( ++( p) == ( pe) )
		goto _test_eof1513;
case 1513:
#line 36371 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1827;
		case 32: goto tr1827;
		case 62: goto tr1829;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1513;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1513;
	} else
		goto st1513;
	goto tr1725;
st1514:
	if ( ++( p) == ( pe) )
		goto _test_eof1514;
case 1514:
	switch( (*( p)) ) {
		case 9: goto st1515;
		case 32: goto st1515;
		case 62: goto tr1831;
	}
	goto tr1725;
tr1844:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1515;
tr1848:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1515;
st1515:
	if ( ++( p) == ( pe) )
		goto _test_eof1515;
case 1515:
#line 36410 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1515;
		case 32: goto st1515;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1832;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1832;
	} else
		goto tr1832;
	goto tr1725;
tr1832:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1516;
st1516:
	if ( ++( p) == ( pe) )
		goto _test_eof1516;
case 1516:
#line 36432 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1833;
		case 32: goto tr1833;
		case 61: goto tr1835;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1516;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1516;
	} else
		goto st1516;
	goto tr1725;
tr1833:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1517;
st1517:
	if ( ++( p) == ( pe) )
		goto _test_eof1517;
case 1517:
#line 36455 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1517;
		case 32: goto st1517;
		case 61: goto st1518;
	}
	goto tr1725;
tr1835:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1518;
st1518:
	if ( ++( p) == ( pe) )
		goto _test_eof1518;
case 1518:
#line 36470 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1518;
		case 32: goto st1518;
		case 34: goto st1519;
		case 39: goto st1522;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1840;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1840;
	} else
		goto tr1840;
	goto tr1725;
st1519:
	if ( ++( p) == ( pe) )
		goto _test_eof1519;
case 1519:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1841;
tr1841:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1520;
st1520:
	if ( ++( p) == ( pe) )
		goto _test_eof1520;
case 1520:
#line 36504 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1843;
	}
	goto st1520;
tr1843:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1521;
st1521:
	if ( ++( p) == ( pe) )
		goto _test_eof1521;
case 1521:
#line 36520 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1844;
		case 32: goto tr1844;
		case 62: goto tr1845;
	}
	goto tr1725;
st1522:
	if ( ++( p) == ( pe) )
		goto _test_eof1522;
case 1522:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1846;
tr1846:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1523;
st1523:
	if ( ++( p) == ( pe) )
		goto _test_eof1523;
case 1523:
#line 36545 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1843;
	}
	goto st1523;
tr1840:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1524;
st1524:
	if ( ++( p) == ( pe) )
		goto _test_eof1524;
case 1524:
#line 36561 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1848;
		case 32: goto tr1848;
		case 62: goto tr1850;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1524;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1524;
	} else
		goto st1524;
	goto tr1725;
st1525:
	if ( ++( p) == ( pe) )
		goto _test_eof1525;
case 1525:
	switch( (*( p)) ) {
		case 9: goto st1526;
		case 32: goto st1526;
		case 62: goto tr1852;
		case 69: goto st1536;
		case 101: goto st1536;
	}
	goto tr1725;
tr1866:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1526;
tr1870:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1526;
st1526:
	if ( ++( p) == ( pe) )
		goto _test_eof1526;
case 1526:
#line 36602 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1526;
		case 32: goto st1526;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1854;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1854;
	} else
		goto tr1854;
	goto tr1725;
tr1854:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1527;
st1527:
	if ( ++( p) == ( pe) )
		goto _test_eof1527;
case 1527:
#line 36624 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1855;
		case 32: goto tr1855;
		case 61: goto tr1857;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1527;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1527;
	} else
		goto st1527;
	goto tr1725;
tr1855:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1528;
st1528:
	if ( ++( p) == ( pe) )
		goto _test_eof1528;
case 1528:
#line 36647 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1528;
		case 32: goto st1528;
		case 61: goto st1529;
	}
	goto tr1725;
tr1857:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1529;
st1529:
	if ( ++( p) == ( pe) )
		goto _test_eof1529;
case 1529:
#line 36662 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1529;
		case 32: goto st1529;
		case 34: goto st1530;
		case 39: goto st1533;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1862;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1862;
	} else
		goto tr1862;
	goto tr1725;
st1530:
	if ( ++( p) == ( pe) )
		goto _test_eof1530;
case 1530:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1863;
tr1863:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1531;
st1531:
	if ( ++( p) == ( pe) )
		goto _test_eof1531;
case 1531:
#line 36696 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1865;
	}
	goto st1531;
tr1865:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1532;
st1532:
	if ( ++( p) == ( pe) )
		goto _test_eof1532;
case 1532:
#line 36712 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1866;
		case 32: goto tr1866;
		case 62: goto tr1867;
	}
	goto tr1725;
st1533:
	if ( ++( p) == ( pe) )
		goto _test_eof1533;
case 1533:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1868;
tr1868:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1534;
st1534:
	if ( ++( p) == ( pe) )
		goto _test_eof1534;
case 1534:
#line 36737 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1865;
	}
	goto st1534;
tr1862:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1535;
st1535:
	if ( ++( p) == ( pe) )
		goto _test_eof1535;
case 1535:
#line 36753 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1870;
		case 32: goto tr1870;
		case 62: goto tr1872;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1535;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1535;
	} else
		goto st1535;
	goto tr1725;
st1536:
	if ( ++( p) == ( pe) )
		goto _test_eof1536;
case 1536:
	switch( (*( p)) ) {
		case 65: goto st1537;
		case 97: goto st1537;
	}
	goto tr1725;
st1537:
	if ( ++( p) == ( pe) )
		goto _test_eof1537;
case 1537:
	switch( (*( p)) ) {
		case 68: goto st1538;
		case 100: goto st1538;
	}
	goto tr1725;
st1538:
	if ( ++( p) == ( pe) )
		goto _test_eof1538;
case 1538:
	switch( (*( p)) ) {
		case 9: goto st1539;
		case 32: goto st1539;
		case 62: goto tr1876;
	}
	goto tr1725;
tr1889:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1539;
tr1893:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1539;
st1539:
	if ( ++( p) == ( pe) )
		goto _test_eof1539;
case 1539:
#line 36810 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1539;
		case 32: goto st1539;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1877;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1877;
	} else
		goto tr1877;
	goto tr1725;
tr1877:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1540;
st1540:
	if ( ++( p) == ( pe) )
		goto _test_eof1540;
case 1540:
#line 36832 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1878;
		case 32: goto tr1878;
		case 61: goto tr1880;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1540;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1540;
	} else
		goto st1540;
	goto tr1725;
tr1878:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1541;
st1541:
	if ( ++( p) == ( pe) )
		goto _test_eof1541;
case 1541:
#line 36855 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1541;
		case 32: goto st1541;
		case 61: goto st1542;
	}
	goto tr1725;
tr1880:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1542;
st1542:
	if ( ++( p) == ( pe) )
		goto _test_eof1542;
case 1542:
#line 36870 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1542;
		case 32: goto st1542;
		case 34: goto st1543;
		case 39: goto st1546;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1885;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1885;
	} else
		goto tr1885;
	goto tr1725;
st1543:
	if ( ++( p) == ( pe) )
		goto _test_eof1543;
case 1543:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1886;
tr1886:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1544;
st1544:
	if ( ++( p) == ( pe) )
		goto _test_eof1544;
case 1544:
#line 36904 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1888;
	}
	goto st1544;
tr1888:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1545;
st1545:
	if ( ++( p) == ( pe) )
		goto _test_eof1545;
case 1545:
#line 36920 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1889;
		case 32: goto tr1889;
		case 62: goto tr1890;
	}
	goto tr1725;
st1546:
	if ( ++( p) == ( pe) )
		goto _test_eof1546;
case 1546:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1891;
tr1891:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1547;
st1547:
	if ( ++( p) == ( pe) )
		goto _test_eof1547;
case 1547:
#line 36945 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1888;
	}
	goto st1547;
tr1885:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1548;
st1548:
	if ( ++( p) == ( pe) )
		goto _test_eof1548;
case 1548:
#line 36961 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1893;
		case 32: goto tr1893;
		case 62: goto tr1895;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1548;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1548;
	} else
		goto st1548;
	goto tr1725;
st1549:
	if ( ++( p) == ( pe) )
		goto _test_eof1549;
case 1549:
	switch( (*( p)) ) {
		case 9: goto st1550;
		case 32: goto st1550;
		case 62: goto tr1897;
	}
	goto tr1725;
tr1910:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1550;
tr1914:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1550;
st1550:
	if ( ++( p) == ( pe) )
		goto _test_eof1550;
case 1550:
#line 37000 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1550;
		case 32: goto st1550;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1898;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1898;
	} else
		goto tr1898;
	goto tr1725;
tr1898:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1551;
st1551:
	if ( ++( p) == ( pe) )
		goto _test_eof1551;
case 1551:
#line 37022 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1899;
		case 32: goto tr1899;
		case 61: goto tr1901;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1551;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1551;
	} else
		goto st1551;
	goto tr1725;
tr1899:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1552;
st1552:
	if ( ++( p) == ( pe) )
		goto _test_eof1552;
case 1552:
#line 37045 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1552;
		case 32: goto st1552;
		case 61: goto st1553;
	}
	goto tr1725;
tr1901:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1553;
st1553:
	if ( ++( p) == ( pe) )
		goto _test_eof1553;
case 1553:
#line 37060 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1553;
		case 32: goto st1553;
		case 34: goto st1554;
		case 39: goto st1557;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1906;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1906;
	} else
		goto tr1906;
	goto tr1725;
st1554:
	if ( ++( p) == ( pe) )
		goto _test_eof1554;
case 1554:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1907;
tr1907:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1555;
st1555:
	if ( ++( p) == ( pe) )
		goto _test_eof1555;
case 1555:
#line 37094 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1909;
	}
	goto st1555;
tr1909:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1556;
st1556:
	if ( ++( p) == ( pe) )
		goto _test_eof1556;
case 1556:
#line 37110 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1910;
		case 32: goto tr1910;
		case 62: goto tr1911;
	}
	goto tr1725;
st1557:
	if ( ++( p) == ( pe) )
		goto _test_eof1557;
case 1557:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1912;
tr1912:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1558;
st1558:
	if ( ++( p) == ( pe) )
		goto _test_eof1558;
case 1558:
#line 37135 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1909;
	}
	goto st1558;
tr1906:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1559;
st1559:
	if ( ++( p) == ( pe) )
		goto _test_eof1559;
case 1559:
#line 37151 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1914;
		case 32: goto tr1914;
		case 62: goto tr1916;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1559;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1559;
	} else
		goto st1559;
	goto tr1725;
tr2495:
#line 1 "NONE"
	{( te) = ( p)+1;}
	goto st1941;
st1941:
	if ( ++( p) == ( pe) )
		goto _test_eof1941;
case 1941:
#line 37174 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 47: goto st1560;
		case 67: goto st1583;
		case 84: goto st1611;
		case 99: goto st1583;
		case 116: goto st1611;
	}
	goto tr2496;
st1560:
	if ( ++( p) == ( pe) )
		goto _test_eof1560;
case 1560:
	switch( (*( p)) ) {
		case 67: goto st1561;
		case 84: goto st1569;
		case 99: goto st1561;
		case 116: goto st1569;
	}
	goto tr1725;
st1561:
	if ( ++( p) == ( pe) )
		goto _test_eof1561;
case 1561:
	switch( (*( p)) ) {
		case 79: goto st1562;
		case 111: goto st1562;
	}
	goto tr1725;
st1562:
	if ( ++( p) == ( pe) )
		goto _test_eof1562;
case 1562:
	switch( (*( p)) ) {
		case 76: goto st1563;
		case 108: goto st1563;
	}
	goto tr1725;
st1563:
	if ( ++( p) == ( pe) )
		goto _test_eof1563;
case 1563:
	switch( (*( p)) ) {
		case 71: goto st1564;
		case 103: goto st1564;
	}
	goto tr1725;
st1564:
	if ( ++( p) == ( pe) )
		goto _test_eof1564;
case 1564:
	switch( (*( p)) ) {
		case 82: goto st1565;
		case 114: goto st1565;
	}
	goto tr1725;
st1565:
	if ( ++( p) == ( pe) )
		goto _test_eof1565;
case 1565:
	switch( (*( p)) ) {
		case 79: goto st1566;
		case 111: goto st1566;
	}
	goto tr1725;
st1566:
	if ( ++( p) == ( pe) )
		goto _test_eof1566;
case 1566:
	switch( (*( p)) ) {
		case 85: goto st1567;
		case 117: goto st1567;
	}
	goto tr1725;
st1567:
	if ( ++( p) == ( pe) )
		goto _test_eof1567;
case 1567:
	switch( (*( p)) ) {
		case 80: goto st1568;
		case 112: goto st1568;
	}
	goto tr1725;
st1568:
	if ( ++( p) == ( pe) )
		goto _test_eof1568;
case 1568:
	if ( (*( p)) == 93 )
		goto tr1735;
	goto tr1725;
st1569:
	if ( ++( p) == ( pe) )
		goto _test_eof1569;
case 1569:
	switch( (*( p)) ) {
		case 65: goto st1570;
		case 66: goto st1574;
		case 72: goto st1578;
		case 82: goto st1582;
		case 97: goto st1570;
		case 98: goto st1574;
		case 104: goto st1578;
		case 114: goto st1582;
	}
	goto tr1725;
st1570:
	if ( ++( p) == ( pe) )
		goto _test_eof1570;
case 1570:
	switch( (*( p)) ) {
		case 66: goto st1571;
		case 98: goto st1571;
	}
	goto tr1725;
st1571:
	if ( ++( p) == ( pe) )
		goto _test_eof1571;
case 1571:
	switch( (*( p)) ) {
		case 76: goto st1572;
		case 108: goto st1572;
	}
	goto tr1725;
st1572:
	if ( ++( p) == ( pe) )
		goto _test_eof1572;
case 1572:
	switch( (*( p)) ) {
		case 69: goto st1573;
		case 101: goto st1573;
	}
	goto tr1725;
st1573:
	if ( ++( p) == ( pe) )
		goto _test_eof1573;
case 1573:
	if ( (*( p)) == 93 )
		goto tr1743;
	goto tr1725;
st1574:
	if ( ++( p) == ( pe) )
		goto _test_eof1574;
case 1574:
	switch( (*( p)) ) {
		case 79: goto st1575;
		case 111: goto st1575;
	}
	goto tr1725;
st1575:
	if ( ++( p) == ( pe) )
		goto _test_eof1575;
case 1575:
	switch( (*( p)) ) {
		case 68: goto st1576;
		case 100: goto st1576;
	}
	goto tr1725;
st1576:
	if ( ++( p) == ( pe) )
		goto _test_eof1576;
case 1576:
	switch( (*( p)) ) {
		case 89: goto st1577;
		case 121: goto st1577;
	}
	goto tr1725;
st1577:
	if ( ++( p) == ( pe) )
		goto _test_eof1577;
case 1577:
	if ( (*( p)) == 93 )
		goto tr1747;
	goto tr1725;
st1578:
	if ( ++( p) == ( pe) )
		goto _test_eof1578;
case 1578:
	switch( (*( p)) ) {
		case 69: goto st1579;
		case 101: goto st1579;
	}
	goto tr1725;
st1579:
	if ( ++( p) == ( pe) )
		goto _test_eof1579;
case 1579:
	switch( (*( p)) ) {
		case 65: goto st1580;
		case 97: goto st1580;
	}
	goto tr1725;
st1580:
	if ( ++( p) == ( pe) )
		goto _test_eof1580;
case 1580:
	switch( (*( p)) ) {
		case 68: goto st1581;
		case 100: goto st1581;
	}
	goto tr1725;
st1581:
	if ( ++( p) == ( pe) )
		goto _test_eof1581;
case 1581:
	if ( (*( p)) == 93 )
		goto tr1751;
	goto tr1725;
st1582:
	if ( ++( p) == ( pe) )
		goto _test_eof1582;
case 1582:
	if ( (*( p)) == 93 )
		goto tr1752;
	goto tr1725;
st1583:
	if ( ++( p) == ( pe) )
		goto _test_eof1583;
case 1583:
	switch( (*( p)) ) {
		case 79: goto st1584;
		case 111: goto st1584;
	}
	goto tr1725;
st1584:
	if ( ++( p) == ( pe) )
		goto _test_eof1584;
case 1584:
	switch( (*( p)) ) {
		case 76: goto st1585;
		case 108: goto st1585;
	}
	goto tr1725;
st1585:
	if ( ++( p) == ( pe) )
		goto _test_eof1585;
case 1585:
	switch( (*( p)) ) {
		case 9: goto st1586;
		case 32: goto st1586;
		case 71: goto st1596;
		case 93: goto tr1756;
		case 103: goto st1596;
	}
	goto tr1725;
tr1955:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1586;
tr1958:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1586;
st1586:
	if ( ++( p) == ( pe) )
		goto _test_eof1586;
case 1586:
#line 37432 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1586;
		case 32: goto st1586;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1943;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1943;
	} else
		goto tr1943;
	goto tr1725;
tr1943:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1587;
st1587:
	if ( ++( p) == ( pe) )
		goto _test_eof1587;
case 1587:
#line 37454 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1944;
		case 32: goto tr1944;
		case 61: goto tr1946;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1587;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1587;
	} else
		goto st1587;
	goto tr1725;
tr1944:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1588;
st1588:
	if ( ++( p) == ( pe) )
		goto _test_eof1588;
case 1588:
#line 37477 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1588;
		case 32: goto st1588;
		case 61: goto st1589;
	}
	goto tr1725;
tr1946:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1589;
st1589:
	if ( ++( p) == ( pe) )
		goto _test_eof1589;
case 1589:
#line 37492 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1589;
		case 32: goto st1589;
		case 34: goto st1590;
		case 39: goto st1593;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1951;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1951;
	} else
		goto tr1951;
	goto tr1725;
st1590:
	if ( ++( p) == ( pe) )
		goto _test_eof1590;
case 1590:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1952;
tr1952:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1591;
st1591:
	if ( ++( p) == ( pe) )
		goto _test_eof1591;
case 1591:
#line 37526 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1954;
	}
	goto st1591;
tr1954:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1592;
st1592:
	if ( ++( p) == ( pe) )
		goto _test_eof1592;
case 1592:
#line 37542 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1955;
		case 32: goto tr1955;
		case 93: goto tr1771;
	}
	goto tr1725;
st1593:
	if ( ++( p) == ( pe) )
		goto _test_eof1593;
case 1593:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1956;
tr1956:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1594;
st1594:
	if ( ++( p) == ( pe) )
		goto _test_eof1594;
case 1594:
#line 37567 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1954;
	}
	goto st1594;
tr1951:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1595;
st1595:
	if ( ++( p) == ( pe) )
		goto _test_eof1595;
case 1595:
#line 37583 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1958;
		case 32: goto tr1958;
		case 93: goto tr1776;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1595;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1595;
	} else
		goto st1595;
	goto tr1725;
st1596:
	if ( ++( p) == ( pe) )
		goto _test_eof1596;
case 1596:
	switch( (*( p)) ) {
		case 82: goto st1597;
		case 114: goto st1597;
	}
	goto tr1725;
st1597:
	if ( ++( p) == ( pe) )
		goto _test_eof1597;
case 1597:
	switch( (*( p)) ) {
		case 79: goto st1598;
		case 111: goto st1598;
	}
	goto tr1725;
st1598:
	if ( ++( p) == ( pe) )
		goto _test_eof1598;
case 1598:
	switch( (*( p)) ) {
		case 85: goto st1599;
		case 117: goto st1599;
	}
	goto tr1725;
st1599:
	if ( ++( p) == ( pe) )
		goto _test_eof1599;
case 1599:
	switch( (*( p)) ) {
		case 80: goto st1600;
		case 112: goto st1600;
	}
	goto tr1725;
st1600:
	if ( ++( p) == ( pe) )
		goto _test_eof1600;
case 1600:
	switch( (*( p)) ) {
		case 9: goto st1601;
		case 32: goto st1601;
		case 93: goto tr1782;
	}
	goto tr1725;
tr1977:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1601;
tr1980:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1601;
st1601:
	if ( ++( p) == ( pe) )
		goto _test_eof1601;
case 1601:
#line 37658 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1601;
		case 32: goto st1601;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1965;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1965;
	} else
		goto tr1965;
	goto tr1725;
tr1965:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1602;
st1602:
	if ( ++( p) == ( pe) )
		goto _test_eof1602;
case 1602:
#line 37680 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1966;
		case 32: goto tr1966;
		case 61: goto tr1968;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1602;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1602;
	} else
		goto st1602;
	goto tr1725;
tr1966:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1603;
st1603:
	if ( ++( p) == ( pe) )
		goto _test_eof1603;
case 1603:
#line 37703 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1603;
		case 32: goto st1603;
		case 61: goto st1604;
	}
	goto tr1725;
tr1968:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1604;
st1604:
	if ( ++( p) == ( pe) )
		goto _test_eof1604;
case 1604:
#line 37718 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1604;
		case 32: goto st1604;
		case 34: goto st1605;
		case 39: goto st1608;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1973;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1973;
	} else
		goto tr1973;
	goto tr1725;
st1605:
	if ( ++( p) == ( pe) )
		goto _test_eof1605;
case 1605:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1974;
tr1974:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1606;
st1606:
	if ( ++( p) == ( pe) )
		goto _test_eof1606;
case 1606:
#line 37752 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr1976;
	}
	goto st1606;
tr1976:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1607;
st1607:
	if ( ++( p) == ( pe) )
		goto _test_eof1607;
case 1607:
#line 37768 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1977;
		case 32: goto tr1977;
		case 93: goto tr1796;
	}
	goto tr1725;
st1608:
	if ( ++( p) == ( pe) )
		goto _test_eof1608;
case 1608:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1978;
tr1978:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1609;
st1609:
	if ( ++( p) == ( pe) )
		goto _test_eof1609;
case 1609:
#line 37793 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr1976;
	}
	goto st1609;
tr1973:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1610;
st1610:
	if ( ++( p) == ( pe) )
		goto _test_eof1610;
case 1610:
#line 37809 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1980;
		case 32: goto tr1980;
		case 93: goto tr1801;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1610;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1610;
	} else
		goto st1610;
	goto tr1725;
st1611:
	if ( ++( p) == ( pe) )
		goto _test_eof1611;
case 1611:
	switch( (*( p)) ) {
		case 66: goto st1612;
		case 68: goto st1626;
		case 72: goto st1637;
		case 82: goto st1661;
		case 98: goto st1612;
		case 100: goto st1626;
		case 104: goto st1637;
		case 114: goto st1661;
	}
	goto tr1725;
st1612:
	if ( ++( p) == ( pe) )
		goto _test_eof1612;
case 1612:
	switch( (*( p)) ) {
		case 79: goto st1613;
		case 111: goto st1613;
	}
	goto tr1725;
st1613:
	if ( ++( p) == ( pe) )
		goto _test_eof1613;
case 1613:
	switch( (*( p)) ) {
		case 68: goto st1614;
		case 100: goto st1614;
	}
	goto tr1725;
st1614:
	if ( ++( p) == ( pe) )
		goto _test_eof1614;
case 1614:
	switch( (*( p)) ) {
		case 89: goto st1615;
		case 121: goto st1615;
	}
	goto tr1725;
st1615:
	if ( ++( p) == ( pe) )
		goto _test_eof1615;
case 1615:
	switch( (*( p)) ) {
		case 9: goto st1616;
		case 32: goto st1616;
		case 93: goto tr1810;
	}
	goto tr1725;
tr2002:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1616;
tr2005:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1616;
st1616:
	if ( ++( p) == ( pe) )
		goto _test_eof1616;
case 1616:
#line 37890 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1616;
		case 32: goto st1616;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1990;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1990;
	} else
		goto tr1990;
	goto tr1725;
tr1990:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1617;
st1617:
	if ( ++( p) == ( pe) )
		goto _test_eof1617;
case 1617:
#line 37912 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr1991;
		case 32: goto tr1991;
		case 61: goto tr1993;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1617;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1617;
	} else
		goto st1617;
	goto tr1725;
tr1991:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1618;
st1618:
	if ( ++( p) == ( pe) )
		goto _test_eof1618;
case 1618:
#line 37935 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1618;
		case 32: goto st1618;
		case 61: goto st1619;
	}
	goto tr1725;
tr1993:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1619;
st1619:
	if ( ++( p) == ( pe) )
		goto _test_eof1619;
case 1619:
#line 37950 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1619;
		case 32: goto st1619;
		case 34: goto st1620;
		case 39: goto st1623;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr1998;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr1998;
	} else
		goto tr1998;
	goto tr1725;
st1620:
	if ( ++( p) == ( pe) )
		goto _test_eof1620;
case 1620:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr1999;
tr1999:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1621;
st1621:
	if ( ++( p) == ( pe) )
		goto _test_eof1621;
case 1621:
#line 37984 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr2001;
	}
	goto st1621;
tr2001:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1622;
st1622:
	if ( ++( p) == ( pe) )
		goto _test_eof1622;
case 1622:
#line 38000 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2002;
		case 32: goto tr2002;
		case 93: goto tr1824;
	}
	goto tr1725;
st1623:
	if ( ++( p) == ( pe) )
		goto _test_eof1623;
case 1623:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2003;
tr2003:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1624;
st1624:
	if ( ++( p) == ( pe) )
		goto _test_eof1624;
case 1624:
#line 38025 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr2001;
	}
	goto st1624;
tr1998:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1625;
st1625:
	if ( ++( p) == ( pe) )
		goto _test_eof1625;
case 1625:
#line 38041 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2005;
		case 32: goto tr2005;
		case 93: goto tr1829;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1625;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1625;
	} else
		goto st1625;
	goto tr1725;
st1626:
	if ( ++( p) == ( pe) )
		goto _test_eof1626;
case 1626:
	switch( (*( p)) ) {
		case 9: goto st1627;
		case 32: goto st1627;
		case 93: goto tr1831;
	}
	goto tr1725;
tr2020:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1627;
tr2023:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1627;
st1627:
	if ( ++( p) == ( pe) )
		goto _test_eof1627;
case 1627:
#line 38080 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1627;
		case 32: goto st1627;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2008;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2008;
	} else
		goto tr2008;
	goto tr1725;
tr2008:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1628;
st1628:
	if ( ++( p) == ( pe) )
		goto _test_eof1628;
case 1628:
#line 38102 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2009;
		case 32: goto tr2009;
		case 61: goto tr2011;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1628;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1628;
	} else
		goto st1628;
	goto tr1725;
tr2009:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1629;
st1629:
	if ( ++( p) == ( pe) )
		goto _test_eof1629;
case 1629:
#line 38125 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1629;
		case 32: goto st1629;
		case 61: goto st1630;
	}
	goto tr1725;
tr2011:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1630;
st1630:
	if ( ++( p) == ( pe) )
		goto _test_eof1630;
case 1630:
#line 38140 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1630;
		case 32: goto st1630;
		case 34: goto st1631;
		case 39: goto st1634;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2016;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2016;
	} else
		goto tr2016;
	goto tr1725;
st1631:
	if ( ++( p) == ( pe) )
		goto _test_eof1631;
case 1631:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2017;
tr2017:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1632;
st1632:
	if ( ++( p) == ( pe) )
		goto _test_eof1632;
case 1632:
#line 38174 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr2019;
	}
	goto st1632;
tr2019:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1633;
st1633:
	if ( ++( p) == ( pe) )
		goto _test_eof1633;
case 1633:
#line 38190 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2020;
		case 32: goto tr2020;
		case 93: goto tr1845;
	}
	goto tr1725;
st1634:
	if ( ++( p) == ( pe) )
		goto _test_eof1634;
case 1634:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2021;
tr2021:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1635;
st1635:
	if ( ++( p) == ( pe) )
		goto _test_eof1635;
case 1635:
#line 38215 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr2019;
	}
	goto st1635;
tr2016:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1636;
st1636:
	if ( ++( p) == ( pe) )
		goto _test_eof1636;
case 1636:
#line 38231 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2023;
		case 32: goto tr2023;
		case 93: goto tr1850;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1636;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1636;
	} else
		goto st1636;
	goto tr1725;
st1637:
	if ( ++( p) == ( pe) )
		goto _test_eof1637;
case 1637:
	switch( (*( p)) ) {
		case 9: goto st1638;
		case 32: goto st1638;
		case 69: goto st1648;
		case 93: goto tr1852;
		case 101: goto st1648;
	}
	goto tr1725;
tr2039:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1638;
tr2042:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1638;
st1638:
	if ( ++( p) == ( pe) )
		goto _test_eof1638;
case 1638:
#line 38272 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1638;
		case 32: goto st1638;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2027;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2027;
	} else
		goto tr2027;
	goto tr1725;
tr2027:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1639;
st1639:
	if ( ++( p) == ( pe) )
		goto _test_eof1639;
case 1639:
#line 38294 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2028;
		case 32: goto tr2028;
		case 61: goto tr2030;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1639;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1639;
	} else
		goto st1639;
	goto tr1725;
tr2028:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1640;
st1640:
	if ( ++( p) == ( pe) )
		goto _test_eof1640;
case 1640:
#line 38317 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1640;
		case 32: goto st1640;
		case 61: goto st1641;
	}
	goto tr1725;
tr2030:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1641;
st1641:
	if ( ++( p) == ( pe) )
		goto _test_eof1641;
case 1641:
#line 38332 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1641;
		case 32: goto st1641;
		case 34: goto st1642;
		case 39: goto st1645;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2035;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2035;
	} else
		goto tr2035;
	goto tr1725;
st1642:
	if ( ++( p) == ( pe) )
		goto _test_eof1642;
case 1642:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2036;
tr2036:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1643;
st1643:
	if ( ++( p) == ( pe) )
		goto _test_eof1643;
case 1643:
#line 38366 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr2038;
	}
	goto st1643;
tr2038:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1644;
st1644:
	if ( ++( p) == ( pe) )
		goto _test_eof1644;
case 1644:
#line 38382 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2039;
		case 32: goto tr2039;
		case 93: goto tr1867;
	}
	goto tr1725;
st1645:
	if ( ++( p) == ( pe) )
		goto _test_eof1645;
case 1645:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2040;
tr2040:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1646;
st1646:
	if ( ++( p) == ( pe) )
		goto _test_eof1646;
case 1646:
#line 38407 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr2038;
	}
	goto st1646;
tr2035:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1647;
st1647:
	if ( ++( p) == ( pe) )
		goto _test_eof1647;
case 1647:
#line 38423 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2042;
		case 32: goto tr2042;
		case 93: goto tr1872;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1647;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1647;
	} else
		goto st1647;
	goto tr1725;
st1648:
	if ( ++( p) == ( pe) )
		goto _test_eof1648;
case 1648:
	switch( (*( p)) ) {
		case 65: goto st1649;
		case 97: goto st1649;
	}
	goto tr1725;
st1649:
	if ( ++( p) == ( pe) )
		goto _test_eof1649;
case 1649:
	switch( (*( p)) ) {
		case 68: goto st1650;
		case 100: goto st1650;
	}
	goto tr1725;
st1650:
	if ( ++( p) == ( pe) )
		goto _test_eof1650;
case 1650:
	switch( (*( p)) ) {
		case 9: goto st1651;
		case 32: goto st1651;
		case 93: goto tr1876;
	}
	goto tr1725;
tr2059:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1651;
tr2062:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1651;
st1651:
	if ( ++( p) == ( pe) )
		goto _test_eof1651;
case 1651:
#line 38480 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1651;
		case 32: goto st1651;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2047;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2047;
	} else
		goto tr2047;
	goto tr1725;
tr2047:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1652;
st1652:
	if ( ++( p) == ( pe) )
		goto _test_eof1652;
case 1652:
#line 38502 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2048;
		case 32: goto tr2048;
		case 61: goto tr2050;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1652;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1652;
	} else
		goto st1652;
	goto tr1725;
tr2048:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1653;
st1653:
	if ( ++( p) == ( pe) )
		goto _test_eof1653;
case 1653:
#line 38525 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1653;
		case 32: goto st1653;
		case 61: goto st1654;
	}
	goto tr1725;
tr2050:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1654;
st1654:
	if ( ++( p) == ( pe) )
		goto _test_eof1654;
case 1654:
#line 38540 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1654;
		case 32: goto st1654;
		case 34: goto st1655;
		case 39: goto st1658;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2055;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2055;
	} else
		goto tr2055;
	goto tr1725;
st1655:
	if ( ++( p) == ( pe) )
		goto _test_eof1655;
case 1655:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2056;
tr2056:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1656;
st1656:
	if ( ++( p) == ( pe) )
		goto _test_eof1656;
case 1656:
#line 38574 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr2058;
	}
	goto st1656;
tr2058:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1657;
st1657:
	if ( ++( p) == ( pe) )
		goto _test_eof1657;
case 1657:
#line 38590 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2059;
		case 32: goto tr2059;
		case 93: goto tr1890;
	}
	goto tr1725;
st1658:
	if ( ++( p) == ( pe) )
		goto _test_eof1658;
case 1658:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2060;
tr2060:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1659;
st1659:
	if ( ++( p) == ( pe) )
		goto _test_eof1659;
case 1659:
#line 38615 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr2058;
	}
	goto st1659;
tr2055:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1660;
st1660:
	if ( ++( p) == ( pe) )
		goto _test_eof1660;
case 1660:
#line 38631 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2062;
		case 32: goto tr2062;
		case 93: goto tr1895;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1660;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1660;
	} else
		goto st1660;
	goto tr1725;
st1661:
	if ( ++( p) == ( pe) )
		goto _test_eof1661;
case 1661:
	switch( (*( p)) ) {
		case 9: goto st1662;
		case 32: goto st1662;
		case 93: goto tr1897;
	}
	goto tr1725;
tr2077:
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1662;
tr2080:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
#line 93 "ext/dtext/dtext.cpp.rl"
	{ tag_attributes[{ a1, a2 }] = { b1, b2 }; }
	goto st1662;
st1662:
	if ( ++( p) == ( pe) )
		goto _test_eof1662;
case 1662:
#line 38670 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1662;
		case 32: goto st1662;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2065;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2065;
	} else
		goto tr2065;
	goto tr1725;
tr2065:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ a1 = p; }
	goto st1663;
st1663:
	if ( ++( p) == ( pe) )
		goto _test_eof1663;
case 1663:
#line 38692 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2066;
		case 32: goto tr2066;
		case 61: goto tr2068;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1663;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1663;
	} else
		goto st1663;
	goto tr1725;
tr2066:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1664;
st1664:
	if ( ++( p) == ( pe) )
		goto _test_eof1664;
case 1664:
#line 38715 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1664;
		case 32: goto st1664;
		case 61: goto st1665;
	}
	goto tr1725;
tr2068:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ a2 = p; }
	goto st1665;
st1665:
	if ( ++( p) == ( pe) )
		goto _test_eof1665;
case 1665:
#line 38730 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto st1665;
		case 32: goto st1665;
		case 34: goto st1666;
		case 39: goto st1669;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto tr2073;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto tr2073;
	} else
		goto tr2073;
	goto tr1725;
st1666:
	if ( ++( p) == ( pe) )
		goto _test_eof1666;
case 1666:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2074;
tr2074:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1667;
st1667:
	if ( ++( p) == ( pe) )
		goto _test_eof1667;
case 1667:
#line 38764 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 34: goto tr2076;
	}
	goto st1667;
tr2076:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ b2 = p; }
	goto st1668;
st1668:
	if ( ++( p) == ( pe) )
		goto _test_eof1668;
case 1668:
#line 38780 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2077;
		case 32: goto tr2077;
		case 93: goto tr1911;
	}
	goto tr1725;
st1669:
	if ( ++( p) == ( pe) )
		goto _test_eof1669;
case 1669:
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
	}
	goto tr2078;
tr2078:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1670;
st1670:
	if ( ++( p) == ( pe) )
		goto _test_eof1670;
case 1670:
#line 38805 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 0: goto tr1725;
		case 10: goto tr1725;
		case 13: goto tr1725;
		case 39: goto tr2076;
	}
	goto st1670;
tr2073:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ b1 = p; }
	goto st1671;
st1671:
	if ( ++( p) == ( pe) )
		goto _test_eof1671;
case 1671:
#line 38821 "ext/dtext/dtext.cpp"
	switch( (*( p)) ) {
		case 9: goto tr2080;
		case 32: goto tr2080;
		case 93: goto tr1916;
	}
	if ( (*( p)) < 65 ) {
		if ( 48 <= (*( p)) && (*( p)) <= 57 )
			goto st1671;
	} else if ( (*( p)) > 90 ) {
		if ( 97 <= (*( p)) && (*( p)) <= 122 )
			goto st1671;
	} else
		goto st1671;
	goto tr1725;
	}
	_test_eof1672: ( cs) = 1672; goto _test_eof; 
	_test_eof1673: ( cs) = 1673; goto _test_eof; 
	_test_eof1: ( cs) = 1; goto _test_eof; 
	_test_eof1674: ( cs) = 1674; goto _test_eof; 
	_test_eof2: ( cs) = 2; goto _test_eof; 
	_test_eof1675: ( cs) = 1675; goto _test_eof; 
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
	_test_eof1676: ( cs) = 1676; goto _test_eof; 
	_test_eof17: ( cs) = 17; goto _test_eof; 
	_test_eof18: ( cs) = 18; goto _test_eof; 
	_test_eof19: ( cs) = 19; goto _test_eof; 
	_test_eof20: ( cs) = 20; goto _test_eof; 
	_test_eof21: ( cs) = 21; goto _test_eof; 
	_test_eof22: ( cs) = 22; goto _test_eof; 
	_test_eof1677: ( cs) = 1677; goto _test_eof; 
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
	_test_eof1678: ( cs) = 1678; goto _test_eof; 
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
	_test_eof1679: ( cs) = 1679; goto _test_eof; 
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
	_test_eof1680: ( cs) = 1680; goto _test_eof; 
	_test_eof1681: ( cs) = 1681; goto _test_eof; 
	_test_eof75: ( cs) = 75; goto _test_eof; 
	_test_eof1682: ( cs) = 1682; goto _test_eof; 
	_test_eof1683: ( cs) = 1683; goto _test_eof; 
	_test_eof76: ( cs) = 76; goto _test_eof; 
	_test_eof1684: ( cs) = 1684; goto _test_eof; 
	_test_eof77: ( cs) = 77; goto _test_eof; 
	_test_eof78: ( cs) = 78; goto _test_eof; 
	_test_eof79: ( cs) = 79; goto _test_eof; 
	_test_eof1685: ( cs) = 1685; goto _test_eof; 
	_test_eof1686: ( cs) = 1686; goto _test_eof; 
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
	_test_eof1687: ( cs) = 1687; goto _test_eof; 
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
	_test_eof1688: ( cs) = 1688; goto _test_eof; 
	_test_eof100: ( cs) = 100; goto _test_eof; 
	_test_eof101: ( cs) = 101; goto _test_eof; 
	_test_eof102: ( cs) = 102; goto _test_eof; 
	_test_eof103: ( cs) = 103; goto _test_eof; 
	_test_eof104: ( cs) = 104; goto _test_eof; 
	_test_eof105: ( cs) = 105; goto _test_eof; 
	_test_eof106: ( cs) = 106; goto _test_eof; 
	_test_eof1689: ( cs) = 1689; goto _test_eof; 
	_test_eof107: ( cs) = 107; goto _test_eof; 
	_test_eof1690: ( cs) = 1690; goto _test_eof; 
	_test_eof108: ( cs) = 108; goto _test_eof; 
	_test_eof109: ( cs) = 109; goto _test_eof; 
	_test_eof110: ( cs) = 110; goto _test_eof; 
	_test_eof111: ( cs) = 111; goto _test_eof; 
	_test_eof112: ( cs) = 112; goto _test_eof; 
	_test_eof113: ( cs) = 113; goto _test_eof; 
	_test_eof114: ( cs) = 114; goto _test_eof; 
	_test_eof115: ( cs) = 115; goto _test_eof; 
	_test_eof116: ( cs) = 116; goto _test_eof; 
	_test_eof1691: ( cs) = 1691; goto _test_eof; 
	_test_eof117: ( cs) = 117; goto _test_eof; 
	_test_eof1692: ( cs) = 1692; goto _test_eof; 
	_test_eof118: ( cs) = 118; goto _test_eof; 
	_test_eof119: ( cs) = 119; goto _test_eof; 
	_test_eof120: ( cs) = 120; goto _test_eof; 
	_test_eof121: ( cs) = 121; goto _test_eof; 
	_test_eof122: ( cs) = 122; goto _test_eof; 
	_test_eof123: ( cs) = 123; goto _test_eof; 
	_test_eof124: ( cs) = 124; goto _test_eof; 
	_test_eof1693: ( cs) = 1693; goto _test_eof; 
	_test_eof125: ( cs) = 125; goto _test_eof; 
	_test_eof126: ( cs) = 126; goto _test_eof; 
	_test_eof127: ( cs) = 127; goto _test_eof; 
	_test_eof128: ( cs) = 128; goto _test_eof; 
	_test_eof129: ( cs) = 129; goto _test_eof; 
	_test_eof130: ( cs) = 130; goto _test_eof; 
	_test_eof131: ( cs) = 131; goto _test_eof; 
	_test_eof132: ( cs) = 132; goto _test_eof; 
	_test_eof1694: ( cs) = 1694; goto _test_eof; 
	_test_eof133: ( cs) = 133; goto _test_eof; 
	_test_eof134: ( cs) = 134; goto _test_eof; 
	_test_eof135: ( cs) = 135; goto _test_eof; 
	_test_eof1695: ( cs) = 1695; goto _test_eof; 
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
	_test_eof1696: ( cs) = 1696; goto _test_eof; 
	_test_eof1697: ( cs) = 1697; goto _test_eof; 
	_test_eof1698: ( cs) = 1698; goto _test_eof; 
	_test_eof1699: ( cs) = 1699; goto _test_eof; 
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
	_test_eof193: ( cs) = 193; goto _test_eof; 
	_test_eof194: ( cs) = 194; goto _test_eof; 
	_test_eof195: ( cs) = 195; goto _test_eof; 
	_test_eof196: ( cs) = 196; goto _test_eof; 
	_test_eof197: ( cs) = 197; goto _test_eof; 
	_test_eof198: ( cs) = 198; goto _test_eof; 
	_test_eof1700: ( cs) = 1700; goto _test_eof; 
	_test_eof199: ( cs) = 199; goto _test_eof; 
	_test_eof200: ( cs) = 200; goto _test_eof; 
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
	_test_eof1701: ( cs) = 1701; goto _test_eof; 
	_test_eof1702: ( cs) = 1702; goto _test_eof; 
	_test_eof1703: ( cs) = 1703; goto _test_eof; 
	_test_eof214: ( cs) = 214; goto _test_eof; 
	_test_eof215: ( cs) = 215; goto _test_eof; 
	_test_eof216: ( cs) = 216; goto _test_eof; 
	_test_eof1704: ( cs) = 1704; goto _test_eof; 
	_test_eof1705: ( cs) = 1705; goto _test_eof; 
	_test_eof1706: ( cs) = 1706; goto _test_eof; 
	_test_eof217: ( cs) = 217; goto _test_eof; 
	_test_eof1707: ( cs) = 1707; goto _test_eof; 
	_test_eof218: ( cs) = 218; goto _test_eof; 
	_test_eof1708: ( cs) = 1708; goto _test_eof; 
	_test_eof219: ( cs) = 219; goto _test_eof; 
	_test_eof220: ( cs) = 220; goto _test_eof; 
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
	_test_eof1709: ( cs) = 1709; goto _test_eof; 
	_test_eof232: ( cs) = 232; goto _test_eof; 
	_test_eof233: ( cs) = 233; goto _test_eof; 
	_test_eof234: ( cs) = 234; goto _test_eof; 
	_test_eof235: ( cs) = 235; goto _test_eof; 
	_test_eof236: ( cs) = 236; goto _test_eof; 
	_test_eof237: ( cs) = 237; goto _test_eof; 
	_test_eof1710: ( cs) = 1710; goto _test_eof; 
	_test_eof238: ( cs) = 238; goto _test_eof; 
	_test_eof239: ( cs) = 239; goto _test_eof; 
	_test_eof240: ( cs) = 240; goto _test_eof; 
	_test_eof241: ( cs) = 241; goto _test_eof; 
	_test_eof242: ( cs) = 242; goto _test_eof; 
	_test_eof243: ( cs) = 243; goto _test_eof; 
	_test_eof244: ( cs) = 244; goto _test_eof; 
	_test_eof245: ( cs) = 245; goto _test_eof; 
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
	_test_eof1711: ( cs) = 1711; goto _test_eof; 
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
	_test_eof1712: ( cs) = 1712; goto _test_eof; 
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
	_test_eof1713: ( cs) = 1713; goto _test_eof; 
	_test_eof1714: ( cs) = 1714; goto _test_eof; 
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
	_test_eof1715: ( cs) = 1715; goto _test_eof; 
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
	_test_eof1716: ( cs) = 1716; goto _test_eof; 
	_test_eof395: ( cs) = 395; goto _test_eof; 
	_test_eof396: ( cs) = 396; goto _test_eof; 
	_test_eof397: ( cs) = 397; goto _test_eof; 
	_test_eof1717: ( cs) = 1717; goto _test_eof; 
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
	_test_eof1718: ( cs) = 1718; goto _test_eof; 
	_test_eof409: ( cs) = 409; goto _test_eof; 
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
	_test_eof1719: ( cs) = 1719; goto _test_eof; 
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
	_test_eof1720: ( cs) = 1720; goto _test_eof; 
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
	_test_eof454: ( cs) = 454; goto _test_eof; 
	_test_eof1721: ( cs) = 1721; goto _test_eof; 
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
	_test_eof1722: ( cs) = 1722; goto _test_eof; 
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
	_test_eof1723: ( cs) = 1723; goto _test_eof; 
	_test_eof513: ( cs) = 513; goto _test_eof; 
	_test_eof514: ( cs) = 514; goto _test_eof; 
	_test_eof515: ( cs) = 515; goto _test_eof; 
	_test_eof516: ( cs) = 516; goto _test_eof; 
	_test_eof517: ( cs) = 517; goto _test_eof; 
	_test_eof518: ( cs) = 518; goto _test_eof; 
	_test_eof519: ( cs) = 519; goto _test_eof; 
	_test_eof520: ( cs) = 520; goto _test_eof; 
	_test_eof521: ( cs) = 521; goto _test_eof; 
	_test_eof1724: ( cs) = 1724; goto _test_eof; 
	_test_eof1725: ( cs) = 1725; goto _test_eof; 
	_test_eof522: ( cs) = 522; goto _test_eof; 
	_test_eof523: ( cs) = 523; goto _test_eof; 
	_test_eof524: ( cs) = 524; goto _test_eof; 
	_test_eof525: ( cs) = 525; goto _test_eof; 
	_test_eof1726: ( cs) = 1726; goto _test_eof; 
	_test_eof1727: ( cs) = 1727; goto _test_eof; 
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
	_test_eof1728: ( cs) = 1728; goto _test_eof; 
	_test_eof1729: ( cs) = 1729; goto _test_eof; 
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
	_test_eof566: ( cs) = 566; goto _test_eof; 
	_test_eof567: ( cs) = 567; goto _test_eof; 
	_test_eof568: ( cs) = 568; goto _test_eof; 
	_test_eof569: ( cs) = 569; goto _test_eof; 
	_test_eof1730: ( cs) = 1730; goto _test_eof; 
	_test_eof1731: ( cs) = 1731; goto _test_eof; 
	_test_eof570: ( cs) = 570; goto _test_eof; 
	_test_eof571: ( cs) = 571; goto _test_eof; 
	_test_eof572: ( cs) = 572; goto _test_eof; 
	_test_eof573: ( cs) = 573; goto _test_eof; 
	_test_eof1732: ( cs) = 1732; goto _test_eof; 
	_test_eof1733: ( cs) = 1733; goto _test_eof; 
	_test_eof574: ( cs) = 574; goto _test_eof; 
	_test_eof575: ( cs) = 575; goto _test_eof; 
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
	_test_eof1734: ( cs) = 1734; goto _test_eof; 
	_test_eof1735: ( cs) = 1735; goto _test_eof; 
	_test_eof586: ( cs) = 586; goto _test_eof; 
	_test_eof1736: ( cs) = 1736; goto _test_eof; 
	_test_eof1737: ( cs) = 1737; goto _test_eof; 
	_test_eof587: ( cs) = 587; goto _test_eof; 
	_test_eof588: ( cs) = 588; goto _test_eof; 
	_test_eof589: ( cs) = 589; goto _test_eof; 
	_test_eof590: ( cs) = 590; goto _test_eof; 
	_test_eof591: ( cs) = 591; goto _test_eof; 
	_test_eof592: ( cs) = 592; goto _test_eof; 
	_test_eof593: ( cs) = 593; goto _test_eof; 
	_test_eof594: ( cs) = 594; goto _test_eof; 
	_test_eof1738: ( cs) = 1738; goto _test_eof; 
	_test_eof1739: ( cs) = 1739; goto _test_eof; 
	_test_eof595: ( cs) = 595; goto _test_eof; 
	_test_eof1740: ( cs) = 1740; goto _test_eof; 
	_test_eof596: ( cs) = 596; goto _test_eof; 
	_test_eof597: ( cs) = 597; goto _test_eof; 
	_test_eof598: ( cs) = 598; goto _test_eof; 
	_test_eof599: ( cs) = 599; goto _test_eof; 
	_test_eof600: ( cs) = 600; goto _test_eof; 
	_test_eof601: ( cs) = 601; goto _test_eof; 
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
	_test_eof1741: ( cs) = 1741; goto _test_eof; 
	_test_eof614: ( cs) = 614; goto _test_eof; 
	_test_eof615: ( cs) = 615; goto _test_eof; 
	_test_eof616: ( cs) = 616; goto _test_eof; 
	_test_eof617: ( cs) = 617; goto _test_eof; 
	_test_eof618: ( cs) = 618; goto _test_eof; 
	_test_eof619: ( cs) = 619; goto _test_eof; 
	_test_eof620: ( cs) = 620; goto _test_eof; 
	_test_eof621: ( cs) = 621; goto _test_eof; 
	_test_eof622: ( cs) = 622; goto _test_eof; 
	_test_eof1742: ( cs) = 1742; goto _test_eof; 
	_test_eof1743: ( cs) = 1743; goto _test_eof; 
	_test_eof1744: ( cs) = 1744; goto _test_eof; 
	_test_eof1745: ( cs) = 1745; goto _test_eof; 
	_test_eof1746: ( cs) = 1746; goto _test_eof; 
	_test_eof623: ( cs) = 623; goto _test_eof; 
	_test_eof624: ( cs) = 624; goto _test_eof; 
	_test_eof1747: ( cs) = 1747; goto _test_eof; 
	_test_eof1748: ( cs) = 1748; goto _test_eof; 
	_test_eof1749: ( cs) = 1749; goto _test_eof; 
	_test_eof1750: ( cs) = 1750; goto _test_eof; 
	_test_eof1751: ( cs) = 1751; goto _test_eof; 
	_test_eof1752: ( cs) = 1752; goto _test_eof; 
	_test_eof625: ( cs) = 625; goto _test_eof; 
	_test_eof626: ( cs) = 626; goto _test_eof; 
	_test_eof1753: ( cs) = 1753; goto _test_eof; 
	_test_eof627: ( cs) = 627; goto _test_eof; 
	_test_eof628: ( cs) = 628; goto _test_eof; 
	_test_eof629: ( cs) = 629; goto _test_eof; 
	_test_eof630: ( cs) = 630; goto _test_eof; 
	_test_eof631: ( cs) = 631; goto _test_eof; 
	_test_eof632: ( cs) = 632; goto _test_eof; 
	_test_eof633: ( cs) = 633; goto _test_eof; 
	_test_eof634: ( cs) = 634; goto _test_eof; 
	_test_eof635: ( cs) = 635; goto _test_eof; 
	_test_eof1754: ( cs) = 1754; goto _test_eof; 
	_test_eof1755: ( cs) = 1755; goto _test_eof; 
	_test_eof1756: ( cs) = 1756; goto _test_eof; 
	_test_eof1757: ( cs) = 1757; goto _test_eof; 
	_test_eof1758: ( cs) = 1758; goto _test_eof; 
	_test_eof636: ( cs) = 636; goto _test_eof; 
	_test_eof637: ( cs) = 637; goto _test_eof; 
	_test_eof638: ( cs) = 638; goto _test_eof; 
	_test_eof639: ( cs) = 639; goto _test_eof; 
	_test_eof640: ( cs) = 640; goto _test_eof; 
	_test_eof641: ( cs) = 641; goto _test_eof; 
	_test_eof642: ( cs) = 642; goto _test_eof; 
	_test_eof643: ( cs) = 643; goto _test_eof; 
	_test_eof644: ( cs) = 644; goto _test_eof; 
	_test_eof645: ( cs) = 645; goto _test_eof; 
	_test_eof1759: ( cs) = 1759; goto _test_eof; 
	_test_eof1760: ( cs) = 1760; goto _test_eof; 
	_test_eof1761: ( cs) = 1761; goto _test_eof; 
	_test_eof1762: ( cs) = 1762; goto _test_eof; 
	_test_eof646: ( cs) = 646; goto _test_eof; 
	_test_eof647: ( cs) = 647; goto _test_eof; 
	_test_eof1763: ( cs) = 1763; goto _test_eof; 
	_test_eof1764: ( cs) = 1764; goto _test_eof; 
	_test_eof1765: ( cs) = 1765; goto _test_eof; 
	_test_eof648: ( cs) = 648; goto _test_eof; 
	_test_eof649: ( cs) = 649; goto _test_eof; 
	_test_eof1766: ( cs) = 1766; goto _test_eof; 
	_test_eof1767: ( cs) = 1767; goto _test_eof; 
	_test_eof1768: ( cs) = 1768; goto _test_eof; 
	_test_eof1769: ( cs) = 1769; goto _test_eof; 
	_test_eof1770: ( cs) = 1770; goto _test_eof; 
	_test_eof1771: ( cs) = 1771; goto _test_eof; 
	_test_eof1772: ( cs) = 1772; goto _test_eof; 
	_test_eof1773: ( cs) = 1773; goto _test_eof; 
	_test_eof650: ( cs) = 650; goto _test_eof; 
	_test_eof651: ( cs) = 651; goto _test_eof; 
	_test_eof1774: ( cs) = 1774; goto _test_eof; 
	_test_eof1775: ( cs) = 1775; goto _test_eof; 
	_test_eof1776: ( cs) = 1776; goto _test_eof; 
	_test_eof652: ( cs) = 652; goto _test_eof; 
	_test_eof653: ( cs) = 653; goto _test_eof; 
	_test_eof1777: ( cs) = 1777; goto _test_eof; 
	_test_eof1778: ( cs) = 1778; goto _test_eof; 
	_test_eof1779: ( cs) = 1779; goto _test_eof; 
	_test_eof1780: ( cs) = 1780; goto _test_eof; 
	_test_eof1781: ( cs) = 1781; goto _test_eof; 
	_test_eof1782: ( cs) = 1782; goto _test_eof; 
	_test_eof654: ( cs) = 654; goto _test_eof; 
	_test_eof655: ( cs) = 655; goto _test_eof; 
	_test_eof1783: ( cs) = 1783; goto _test_eof; 
	_test_eof656: ( cs) = 656; goto _test_eof; 
	_test_eof1784: ( cs) = 1784; goto _test_eof; 
	_test_eof1785: ( cs) = 1785; goto _test_eof; 
	_test_eof1786: ( cs) = 1786; goto _test_eof; 
	_test_eof657: ( cs) = 657; goto _test_eof; 
	_test_eof658: ( cs) = 658; goto _test_eof; 
	_test_eof1787: ( cs) = 1787; goto _test_eof; 
	_test_eof1788: ( cs) = 1788; goto _test_eof; 
	_test_eof1789: ( cs) = 1789; goto _test_eof; 
	_test_eof1790: ( cs) = 1790; goto _test_eof; 
	_test_eof1791: ( cs) = 1791; goto _test_eof; 
	_test_eof659: ( cs) = 659; goto _test_eof; 
	_test_eof660: ( cs) = 660; goto _test_eof; 
	_test_eof1792: ( cs) = 1792; goto _test_eof; 
	_test_eof1793: ( cs) = 1793; goto _test_eof; 
	_test_eof1794: ( cs) = 1794; goto _test_eof; 
	_test_eof1795: ( cs) = 1795; goto _test_eof; 
	_test_eof1796: ( cs) = 1796; goto _test_eof; 
	_test_eof661: ( cs) = 661; goto _test_eof; 
	_test_eof662: ( cs) = 662; goto _test_eof; 
	_test_eof663: ( cs) = 663; goto _test_eof; 
	_test_eof1797: ( cs) = 1797; goto _test_eof; 
	_test_eof664: ( cs) = 664; goto _test_eof; 
	_test_eof665: ( cs) = 665; goto _test_eof; 
	_test_eof666: ( cs) = 666; goto _test_eof; 
	_test_eof667: ( cs) = 667; goto _test_eof; 
	_test_eof668: ( cs) = 668; goto _test_eof; 
	_test_eof669: ( cs) = 669; goto _test_eof; 
	_test_eof670: ( cs) = 670; goto _test_eof; 
	_test_eof671: ( cs) = 671; goto _test_eof; 
	_test_eof672: ( cs) = 672; goto _test_eof; 
	_test_eof673: ( cs) = 673; goto _test_eof; 
	_test_eof674: ( cs) = 674; goto _test_eof; 
	_test_eof1798: ( cs) = 1798; goto _test_eof; 
	_test_eof675: ( cs) = 675; goto _test_eof; 
	_test_eof676: ( cs) = 676; goto _test_eof; 
	_test_eof1799: ( cs) = 1799; goto _test_eof; 
	_test_eof1800: ( cs) = 1800; goto _test_eof; 
	_test_eof1801: ( cs) = 1801; goto _test_eof; 
	_test_eof1802: ( cs) = 1802; goto _test_eof; 
	_test_eof1803: ( cs) = 1803; goto _test_eof; 
	_test_eof1804: ( cs) = 1804; goto _test_eof; 
	_test_eof1805: ( cs) = 1805; goto _test_eof; 
	_test_eof1806: ( cs) = 1806; goto _test_eof; 
	_test_eof1807: ( cs) = 1807; goto _test_eof; 
	_test_eof677: ( cs) = 677; goto _test_eof; 
	_test_eof678: ( cs) = 678; goto _test_eof; 
	_test_eof679: ( cs) = 679; goto _test_eof; 
	_test_eof680: ( cs) = 680; goto _test_eof; 
	_test_eof681: ( cs) = 681; goto _test_eof; 
	_test_eof682: ( cs) = 682; goto _test_eof; 
	_test_eof683: ( cs) = 683; goto _test_eof; 
	_test_eof684: ( cs) = 684; goto _test_eof; 
	_test_eof685: ( cs) = 685; goto _test_eof; 
	_test_eof1808: ( cs) = 1808; goto _test_eof; 
	_test_eof686: ( cs) = 686; goto _test_eof; 
	_test_eof687: ( cs) = 687; goto _test_eof; 
	_test_eof688: ( cs) = 688; goto _test_eof; 
	_test_eof689: ( cs) = 689; goto _test_eof; 
	_test_eof690: ( cs) = 690; goto _test_eof; 
	_test_eof691: ( cs) = 691; goto _test_eof; 
	_test_eof692: ( cs) = 692; goto _test_eof; 
	_test_eof693: ( cs) = 693; goto _test_eof; 
	_test_eof694: ( cs) = 694; goto _test_eof; 
	_test_eof695: ( cs) = 695; goto _test_eof; 
	_test_eof1809: ( cs) = 1809; goto _test_eof; 
	_test_eof696: ( cs) = 696; goto _test_eof; 
	_test_eof697: ( cs) = 697; goto _test_eof; 
	_test_eof698: ( cs) = 698; goto _test_eof; 
	_test_eof699: ( cs) = 699; goto _test_eof; 
	_test_eof700: ( cs) = 700; goto _test_eof; 
	_test_eof701: ( cs) = 701; goto _test_eof; 
	_test_eof702: ( cs) = 702; goto _test_eof; 
	_test_eof703: ( cs) = 703; goto _test_eof; 
	_test_eof704: ( cs) = 704; goto _test_eof; 
	_test_eof705: ( cs) = 705; goto _test_eof; 
	_test_eof706: ( cs) = 706; goto _test_eof; 
	_test_eof1810: ( cs) = 1810; goto _test_eof; 
	_test_eof707: ( cs) = 707; goto _test_eof; 
	_test_eof708: ( cs) = 708; goto _test_eof; 
	_test_eof709: ( cs) = 709; goto _test_eof; 
	_test_eof710: ( cs) = 710; goto _test_eof; 
	_test_eof711: ( cs) = 711; goto _test_eof; 
	_test_eof712: ( cs) = 712; goto _test_eof; 
	_test_eof713: ( cs) = 713; goto _test_eof; 
	_test_eof714: ( cs) = 714; goto _test_eof; 
	_test_eof715: ( cs) = 715; goto _test_eof; 
	_test_eof716: ( cs) = 716; goto _test_eof; 
	_test_eof717: ( cs) = 717; goto _test_eof; 
	_test_eof718: ( cs) = 718; goto _test_eof; 
	_test_eof719: ( cs) = 719; goto _test_eof; 
	_test_eof1811: ( cs) = 1811; goto _test_eof; 
	_test_eof720: ( cs) = 720; goto _test_eof; 
	_test_eof721: ( cs) = 721; goto _test_eof; 
	_test_eof722: ( cs) = 722; goto _test_eof; 
	_test_eof723: ( cs) = 723; goto _test_eof; 
	_test_eof724: ( cs) = 724; goto _test_eof; 
	_test_eof725: ( cs) = 725; goto _test_eof; 
	_test_eof726: ( cs) = 726; goto _test_eof; 
	_test_eof727: ( cs) = 727; goto _test_eof; 
	_test_eof728: ( cs) = 728; goto _test_eof; 
	_test_eof729: ( cs) = 729; goto _test_eof; 
	_test_eof1812: ( cs) = 1812; goto _test_eof; 
	_test_eof1813: ( cs) = 1813; goto _test_eof; 
	_test_eof1814: ( cs) = 1814; goto _test_eof; 
	_test_eof1815: ( cs) = 1815; goto _test_eof; 
	_test_eof1816: ( cs) = 1816; goto _test_eof; 
	_test_eof1817: ( cs) = 1817; goto _test_eof; 
	_test_eof1818: ( cs) = 1818; goto _test_eof; 
	_test_eof1819: ( cs) = 1819; goto _test_eof; 
	_test_eof1820: ( cs) = 1820; goto _test_eof; 
	_test_eof1821: ( cs) = 1821; goto _test_eof; 
	_test_eof1822: ( cs) = 1822; goto _test_eof; 
	_test_eof1823: ( cs) = 1823; goto _test_eof; 
	_test_eof1824: ( cs) = 1824; goto _test_eof; 
	_test_eof730: ( cs) = 730; goto _test_eof; 
	_test_eof731: ( cs) = 731; goto _test_eof; 
	_test_eof1825: ( cs) = 1825; goto _test_eof; 
	_test_eof1826: ( cs) = 1826; goto _test_eof; 
	_test_eof1827: ( cs) = 1827; goto _test_eof; 
	_test_eof1828: ( cs) = 1828; goto _test_eof; 
	_test_eof1829: ( cs) = 1829; goto _test_eof; 
	_test_eof732: ( cs) = 732; goto _test_eof; 
	_test_eof733: ( cs) = 733; goto _test_eof; 
	_test_eof1830: ( cs) = 1830; goto _test_eof; 
	_test_eof1831: ( cs) = 1831; goto _test_eof; 
	_test_eof1832: ( cs) = 1832; goto _test_eof; 
	_test_eof1833: ( cs) = 1833; goto _test_eof; 
	_test_eof734: ( cs) = 734; goto _test_eof; 
	_test_eof735: ( cs) = 735; goto _test_eof; 
	_test_eof736: ( cs) = 736; goto _test_eof; 
	_test_eof737: ( cs) = 737; goto _test_eof; 
	_test_eof738: ( cs) = 738; goto _test_eof; 
	_test_eof739: ( cs) = 739; goto _test_eof; 
	_test_eof740: ( cs) = 740; goto _test_eof; 
	_test_eof741: ( cs) = 741; goto _test_eof; 
	_test_eof742: ( cs) = 742; goto _test_eof; 
	_test_eof1834: ( cs) = 1834; goto _test_eof; 
	_test_eof1835: ( cs) = 1835; goto _test_eof; 
	_test_eof1836: ( cs) = 1836; goto _test_eof; 
	_test_eof1837: ( cs) = 1837; goto _test_eof; 
	_test_eof1838: ( cs) = 1838; goto _test_eof; 
	_test_eof743: ( cs) = 743; goto _test_eof; 
	_test_eof744: ( cs) = 744; goto _test_eof; 
	_test_eof1839: ( cs) = 1839; goto _test_eof; 
	_test_eof1840: ( cs) = 1840; goto _test_eof; 
	_test_eof1841: ( cs) = 1841; goto _test_eof; 
	_test_eof1842: ( cs) = 1842; goto _test_eof; 
	_test_eof1843: ( cs) = 1843; goto _test_eof; 
	_test_eof745: ( cs) = 745; goto _test_eof; 
	_test_eof746: ( cs) = 746; goto _test_eof; 
	_test_eof1844: ( cs) = 1844; goto _test_eof; 
	_test_eof1845: ( cs) = 1845; goto _test_eof; 
	_test_eof1846: ( cs) = 1846; goto _test_eof; 
	_test_eof747: ( cs) = 747; goto _test_eof; 
	_test_eof748: ( cs) = 748; goto _test_eof; 
	_test_eof1847: ( cs) = 1847; goto _test_eof; 
	_test_eof749: ( cs) = 749; goto _test_eof; 
	_test_eof750: ( cs) = 750; goto _test_eof; 
	_test_eof751: ( cs) = 751; goto _test_eof; 
	_test_eof752: ( cs) = 752; goto _test_eof; 
	_test_eof753: ( cs) = 753; goto _test_eof; 
	_test_eof754: ( cs) = 754; goto _test_eof; 
	_test_eof755: ( cs) = 755; goto _test_eof; 
	_test_eof756: ( cs) = 756; goto _test_eof; 
	_test_eof757: ( cs) = 757; goto _test_eof; 
	_test_eof1848: ( cs) = 1848; goto _test_eof; 
	_test_eof1849: ( cs) = 1849; goto _test_eof; 
	_test_eof1850: ( cs) = 1850; goto _test_eof; 
	_test_eof1851: ( cs) = 1851; goto _test_eof; 
	_test_eof758: ( cs) = 758; goto _test_eof; 
	_test_eof759: ( cs) = 759; goto _test_eof; 
	_test_eof1852: ( cs) = 1852; goto _test_eof; 
	_test_eof1853: ( cs) = 1853; goto _test_eof; 
	_test_eof1854: ( cs) = 1854; goto _test_eof; 
	_test_eof1855: ( cs) = 1855; goto _test_eof; 
	_test_eof1856: ( cs) = 1856; goto _test_eof; 
	_test_eof1857: ( cs) = 1857; goto _test_eof; 
	_test_eof1858: ( cs) = 1858; goto _test_eof; 
	_test_eof760: ( cs) = 760; goto _test_eof; 
	_test_eof761: ( cs) = 761; goto _test_eof; 
	_test_eof1859: ( cs) = 1859; goto _test_eof; 
	_test_eof1860: ( cs) = 1860; goto _test_eof; 
	_test_eof1861: ( cs) = 1861; goto _test_eof; 
	_test_eof1862: ( cs) = 1862; goto _test_eof; 
	_test_eof762: ( cs) = 762; goto _test_eof; 
	_test_eof763: ( cs) = 763; goto _test_eof; 
	_test_eof1863: ( cs) = 1863; goto _test_eof; 
	_test_eof1864: ( cs) = 1864; goto _test_eof; 
	_test_eof1865: ( cs) = 1865; goto _test_eof; 
	_test_eof1866: ( cs) = 1866; goto _test_eof; 
	_test_eof1867: ( cs) = 1867; goto _test_eof; 
	_test_eof764: ( cs) = 764; goto _test_eof; 
	_test_eof765: ( cs) = 765; goto _test_eof; 
	_test_eof766: ( cs) = 766; goto _test_eof; 
	_test_eof767: ( cs) = 767; goto _test_eof; 
	_test_eof768: ( cs) = 768; goto _test_eof; 
	_test_eof769: ( cs) = 769; goto _test_eof; 
	_test_eof770: ( cs) = 770; goto _test_eof; 
	_test_eof1868: ( cs) = 1868; goto _test_eof; 
	_test_eof771: ( cs) = 771; goto _test_eof; 
	_test_eof772: ( cs) = 772; goto _test_eof; 
	_test_eof773: ( cs) = 773; goto _test_eof; 
	_test_eof774: ( cs) = 774; goto _test_eof; 
	_test_eof775: ( cs) = 775; goto _test_eof; 
	_test_eof776: ( cs) = 776; goto _test_eof; 
	_test_eof777: ( cs) = 777; goto _test_eof; 
	_test_eof778: ( cs) = 778; goto _test_eof; 
	_test_eof1869: ( cs) = 1869; goto _test_eof; 
	_test_eof1870: ( cs) = 1870; goto _test_eof; 
	_test_eof1871: ( cs) = 1871; goto _test_eof; 
	_test_eof1872: ( cs) = 1872; goto _test_eof; 
	_test_eof1873: ( cs) = 1873; goto _test_eof; 
	_test_eof1874: ( cs) = 1874; goto _test_eof; 
	_test_eof1875: ( cs) = 1875; goto _test_eof; 
	_test_eof1876: ( cs) = 1876; goto _test_eof; 
	_test_eof779: ( cs) = 779; goto _test_eof; 
	_test_eof780: ( cs) = 780; goto _test_eof; 
	_test_eof1877: ( cs) = 1877; goto _test_eof; 
	_test_eof1878: ( cs) = 1878; goto _test_eof; 
	_test_eof1879: ( cs) = 1879; goto _test_eof; 
	_test_eof1880: ( cs) = 1880; goto _test_eof; 
	_test_eof1881: ( cs) = 1881; goto _test_eof; 
	_test_eof1882: ( cs) = 1882; goto _test_eof; 
	_test_eof781: ( cs) = 781; goto _test_eof; 
	_test_eof782: ( cs) = 782; goto _test_eof; 
	_test_eof1883: ( cs) = 1883; goto _test_eof; 
	_test_eof1884: ( cs) = 1884; goto _test_eof; 
	_test_eof1885: ( cs) = 1885; goto _test_eof; 
	_test_eof1886: ( cs) = 1886; goto _test_eof; 
	_test_eof1887: ( cs) = 1887; goto _test_eof; 
	_test_eof1888: ( cs) = 1888; goto _test_eof; 
	_test_eof1889: ( cs) = 1889; goto _test_eof; 
	_test_eof1890: ( cs) = 1890; goto _test_eof; 
	_test_eof1891: ( cs) = 1891; goto _test_eof; 
	_test_eof783: ( cs) = 783; goto _test_eof; 
	_test_eof784: ( cs) = 784; goto _test_eof; 
	_test_eof1892: ( cs) = 1892; goto _test_eof; 
	_test_eof1893: ( cs) = 1893; goto _test_eof; 
	_test_eof1894: ( cs) = 1894; goto _test_eof; 
	_test_eof1895: ( cs) = 1895; goto _test_eof; 
	_test_eof1896: ( cs) = 1896; goto _test_eof; 
	_test_eof785: ( cs) = 785; goto _test_eof; 
	_test_eof786: ( cs) = 786; goto _test_eof; 
	_test_eof787: ( cs) = 787; goto _test_eof; 
	_test_eof1897: ( cs) = 1897; goto _test_eof; 
	_test_eof788: ( cs) = 788; goto _test_eof; 
	_test_eof789: ( cs) = 789; goto _test_eof; 
	_test_eof790: ( cs) = 790; goto _test_eof; 
	_test_eof791: ( cs) = 791; goto _test_eof; 
	_test_eof792: ( cs) = 792; goto _test_eof; 
	_test_eof793: ( cs) = 793; goto _test_eof; 
	_test_eof794: ( cs) = 794; goto _test_eof; 
	_test_eof795: ( cs) = 795; goto _test_eof; 
	_test_eof796: ( cs) = 796; goto _test_eof; 
	_test_eof1898: ( cs) = 1898; goto _test_eof; 
	_test_eof797: ( cs) = 797; goto _test_eof; 
	_test_eof798: ( cs) = 798; goto _test_eof; 
	_test_eof799: ( cs) = 799; goto _test_eof; 
	_test_eof800: ( cs) = 800; goto _test_eof; 
	_test_eof801: ( cs) = 801; goto _test_eof; 
	_test_eof802: ( cs) = 802; goto _test_eof; 
	_test_eof1899: ( cs) = 1899; goto _test_eof; 
	_test_eof1900: ( cs) = 1900; goto _test_eof; 
	_test_eof1901: ( cs) = 1901; goto _test_eof; 
	_test_eof1902: ( cs) = 1902; goto _test_eof; 
	_test_eof1903: ( cs) = 1903; goto _test_eof; 
	_test_eof1904: ( cs) = 1904; goto _test_eof; 
	_test_eof1905: ( cs) = 1905; goto _test_eof; 
	_test_eof1906: ( cs) = 1906; goto _test_eof; 
	_test_eof1907: ( cs) = 1907; goto _test_eof; 
	_test_eof1908: ( cs) = 1908; goto _test_eof; 
	_test_eof1909: ( cs) = 1909; goto _test_eof; 
	_test_eof1910: ( cs) = 1910; goto _test_eof; 
	_test_eof1911: ( cs) = 1911; goto _test_eof; 
	_test_eof803: ( cs) = 803; goto _test_eof; 
	_test_eof804: ( cs) = 804; goto _test_eof; 
	_test_eof805: ( cs) = 805; goto _test_eof; 
	_test_eof806: ( cs) = 806; goto _test_eof; 
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
	_test_eof821: ( cs) = 821; goto _test_eof; 
	_test_eof822: ( cs) = 822; goto _test_eof; 
	_test_eof823: ( cs) = 823; goto _test_eof; 
	_test_eof824: ( cs) = 824; goto _test_eof; 
	_test_eof825: ( cs) = 825; goto _test_eof; 
	_test_eof826: ( cs) = 826; goto _test_eof; 
	_test_eof827: ( cs) = 827; goto _test_eof; 
	_test_eof1912: ( cs) = 1912; goto _test_eof; 
	_test_eof828: ( cs) = 828; goto _test_eof; 
	_test_eof829: ( cs) = 829; goto _test_eof; 
	_test_eof1913: ( cs) = 1913; goto _test_eof; 
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
	_test_eof1914: ( cs) = 1914; goto _test_eof; 
	_test_eof895: ( cs) = 895; goto _test_eof; 
	_test_eof1915: ( cs) = 1915; goto _test_eof; 
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
	_test_eof1916: ( cs) = 1916; goto _test_eof; 
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
	_test_eof1917: ( cs) = 1917; goto _test_eof; 
	_test_eof1074: ( cs) = 1074; goto _test_eof; 
	_test_eof1075: ( cs) = 1075; goto _test_eof; 
	_test_eof1918: ( cs) = 1918; goto _test_eof; 
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
	_test_eof1919: ( cs) = 1919; goto _test_eof; 
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
	_test_eof1920: ( cs) = 1920; goto _test_eof; 
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
	_test_eof1154: ( cs) = 1154; goto _test_eof; 
	_test_eof1155: ( cs) = 1155; goto _test_eof; 
	_test_eof1156: ( cs) = 1156; goto _test_eof; 
	_test_eof1157: ( cs) = 1157; goto _test_eof; 
	_test_eof1158: ( cs) = 1158; goto _test_eof; 
	_test_eof1921: ( cs) = 1921; goto _test_eof; 
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
	_test_eof1181: ( cs) = 1181; goto _test_eof; 
	_test_eof1922: ( cs) = 1922; goto _test_eof; 
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
	_test_eof1923: ( cs) = 1923; goto _test_eof; 
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
	_test_eof1924: ( cs) = 1924; goto _test_eof; 
	_test_eof1925: ( cs) = 1925; goto _test_eof; 
	_test_eof1299: ( cs) = 1299; goto _test_eof; 
	_test_eof1300: ( cs) = 1300; goto _test_eof; 
	_test_eof1301: ( cs) = 1301; goto _test_eof; 
	_test_eof1302: ( cs) = 1302; goto _test_eof; 
	_test_eof1303: ( cs) = 1303; goto _test_eof; 
	_test_eof1304: ( cs) = 1304; goto _test_eof; 
	_test_eof1305: ( cs) = 1305; goto _test_eof; 
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
	_test_eof1926: ( cs) = 1926; goto _test_eof; 
	_test_eof1345: ( cs) = 1345; goto _test_eof; 
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
	_test_eof1927: ( cs) = 1927; goto _test_eof; 
	_test_eof1382: ( cs) = 1382; goto _test_eof; 
	_test_eof1383: ( cs) = 1383; goto _test_eof; 
	_test_eof1928: ( cs) = 1928; goto _test_eof; 
	_test_eof1384: ( cs) = 1384; goto _test_eof; 
	_test_eof1385: ( cs) = 1385; goto _test_eof; 
	_test_eof1386: ( cs) = 1386; goto _test_eof; 
	_test_eof1387: ( cs) = 1387; goto _test_eof; 
	_test_eof1929: ( cs) = 1929; goto _test_eof; 
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
	_test_eof1930: ( cs) = 1930; goto _test_eof; 
	_test_eof1398: ( cs) = 1398; goto _test_eof; 
	_test_eof1399: ( cs) = 1399; goto _test_eof; 
	_test_eof1400: ( cs) = 1400; goto _test_eof; 
	_test_eof1401: ( cs) = 1401; goto _test_eof; 
	_test_eof1402: ( cs) = 1402; goto _test_eof; 
	_test_eof1403: ( cs) = 1403; goto _test_eof; 
	_test_eof1404: ( cs) = 1404; goto _test_eof; 
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
	_test_eof1931: ( cs) = 1931; goto _test_eof; 
	_test_eof1932: ( cs) = 1932; goto _test_eof; 
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
	_test_eof1933: ( cs) = 1933; goto _test_eof; 
	_test_eof1934: ( cs) = 1934; goto _test_eof; 
	_test_eof1935: ( cs) = 1935; goto _test_eof; 
	_test_eof1936: ( cs) = 1936; goto _test_eof; 
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
	_test_eof1937: ( cs) = 1937; goto _test_eof; 
	_test_eof1938: ( cs) = 1938; goto _test_eof; 
	_test_eof1939: ( cs) = 1939; goto _test_eof; 
	_test_eof1940: ( cs) = 1940; goto _test_eof; 
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
	_test_eof1941: ( cs) = 1941; goto _test_eof; 
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

	_test_eof: {}
	if ( ( p) == ( eof) )
	{
	switch ( ( cs) ) {
	case 1673: goto tr0;
	case 1: goto tr0;
	case 1674: goto tr2093;
	case 2: goto tr3;
	case 1675: goto tr0;
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
	case 1676: goto tr2094;
	case 17: goto tr0;
	case 18: goto tr0;
	case 19: goto tr0;
	case 20: goto tr0;
	case 21: goto tr0;
	case 22: goto tr0;
	case 1677: goto tr2095;
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
	case 1678: goto tr2096;
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
	case 1679: goto tr2097;
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
	case 1680: goto tr2093;
	case 1681: goto tr2093;
	case 75: goto tr3;
	case 1682: goto tr2098;
	case 1683: goto tr2098;
	case 76: goto tr3;
	case 1684: goto tr2093;
	case 77: goto tr3;
	case 78: goto tr3;
	case 79: goto tr3;
	case 1685: goto tr2101;
	case 1686: goto tr2093;
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
	case 1687: goto tr2093;
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
	case 1688: goto tr2110;
	case 100: goto tr3;
	case 101: goto tr3;
	case 102: goto tr3;
	case 103: goto tr3;
	case 104: goto tr3;
	case 105: goto tr3;
	case 106: goto tr3;
	case 1689: goto tr2111;
	case 107: goto tr130;
	case 1690: goto tr2112;
	case 108: goto tr133;
	case 109: goto tr3;
	case 110: goto tr3;
	case 111: goto tr3;
	case 112: goto tr3;
	case 113: goto tr3;
	case 114: goto tr3;
	case 115: goto tr3;
	case 116: goto tr3;
	case 1691: goto tr2113;
	case 117: goto tr3;
	case 1692: goto tr2115;
	case 118: goto tr3;
	case 119: goto tr3;
	case 120: goto tr3;
	case 121: goto tr3;
	case 122: goto tr3;
	case 123: goto tr3;
	case 124: goto tr3;
	case 1693: goto tr2116;
	case 125: goto tr157;
	case 126: goto tr3;
	case 127: goto tr3;
	case 128: goto tr3;
	case 129: goto tr3;
	case 130: goto tr3;
	case 131: goto tr3;
	case 132: goto tr3;
	case 1694: goto tr2117;
	case 133: goto tr3;
	case 134: goto tr3;
	case 135: goto tr3;
	case 1695: goto tr2093;
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
	case 1696: goto tr2093;
	case 1697: goto tr2093;
	case 1699: goto tr2128;
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
	case 193: goto tr206;
	case 194: goto tr206;
	case 195: goto tr206;
	case 196: goto tr206;
	case 197: goto tr206;
	case 198: goto tr206;
	case 1700: goto tr2128;
	case 199: goto tr206;
	case 200: goto tr206;
	case 201: goto tr206;
	case 202: goto tr206;
	case 203: goto tr206;
	case 204: goto tr206;
	case 205: goto tr206;
	case 206: goto tr206;
	case 207: goto tr206;
	case 208: goto tr206;
	case 209: goto tr206;
	case 210: goto tr206;
	case 211: goto tr206;
	case 212: goto tr206;
	case 213: goto tr206;
	case 1702: goto tr2169;
	case 1703: goto tr2170;
	case 214: goto tr250;
	case 215: goto tr250;
	case 216: goto tr253;
	case 1704: goto tr2169;
	case 1705: goto tr2169;
	case 1706: goto tr250;
	case 217: goto tr250;
	case 1707: goto tr2169;
	case 218: goto tr257;
	case 1708: goto tr2172;
	case 219: goto tr259;
	case 220: goto tr259;
	case 221: goto tr259;
	case 222: goto tr259;
	case 223: goto tr250;
	case 224: goto tr250;
	case 225: goto tr250;
	case 226: goto tr250;
	case 227: goto tr250;
	case 228: goto tr250;
	case 229: goto tr250;
	case 230: goto tr250;
	case 231: goto tr250;
	case 1709: goto tr2179;
	case 232: goto tr259;
	case 233: goto tr250;
	case 234: goto tr250;
	case 235: goto tr250;
	case 236: goto tr250;
	case 237: goto tr250;
	case 1710: goto tr2180;
	case 238: goto tr250;
	case 239: goto tr250;
	case 240: goto tr250;
	case 241: goto tr250;
	case 242: goto tr250;
	case 243: goto tr259;
	case 244: goto tr250;
	case 245: goto tr250;
	case 246: goto tr259;
	case 247: goto tr250;
	case 248: goto tr250;
	case 249: goto tr250;
	case 250: goto tr250;
	case 251: goto tr250;
	case 252: goto tr250;
	case 253: goto tr250;
	case 254: goto tr250;
	case 255: goto tr250;
	case 256: goto tr259;
	case 257: goto tr259;
	case 258: goto tr259;
	case 1711: goto tr2181;
	case 259: goto tr259;
	case 260: goto tr259;
	case 261: goto tr259;
	case 262: goto tr259;
	case 263: goto tr259;
	case 264: goto tr259;
	case 265: goto tr259;
	case 266: goto tr259;
	case 267: goto tr259;
	case 268: goto tr259;
	case 269: goto tr259;
	case 270: goto tr259;
	case 271: goto tr259;
	case 272: goto tr259;
	case 273: goto tr259;
	case 274: goto tr259;
	case 275: goto tr259;
	case 276: goto tr259;
	case 277: goto tr259;
	case 278: goto tr259;
	case 279: goto tr259;
	case 280: goto tr259;
	case 281: goto tr259;
	case 282: goto tr259;
	case 283: goto tr259;
	case 284: goto tr259;
	case 285: goto tr259;
	case 286: goto tr259;
	case 287: goto tr259;
	case 288: goto tr259;
	case 289: goto tr259;
	case 290: goto tr259;
	case 291: goto tr259;
	case 292: goto tr259;
	case 293: goto tr259;
	case 1712: goto tr2182;
	case 294: goto tr342;
	case 295: goto tr342;
	case 296: goto tr342;
	case 297: goto tr250;
	case 298: goto tr342;
	case 299: goto tr342;
	case 300: goto tr342;
	case 301: goto tr250;
	case 302: goto tr259;
	case 303: goto tr259;
	case 1713: goto tr2185;
	case 1714: goto tr2185;
	case 304: goto tr259;
	case 305: goto tr259;
	case 306: goto tr259;
	case 307: goto tr250;
	case 308: goto tr250;
	case 309: goto tr250;
	case 310: goto tr250;
	case 311: goto tr250;
	case 312: goto tr250;
	case 313: goto tr250;
	case 314: goto tr250;
	case 315: goto tr259;
	case 316: goto tr259;
	case 317: goto tr259;
	case 318: goto tr259;
	case 319: goto tr259;
	case 320: goto tr259;
	case 321: goto tr259;
	case 322: goto tr259;
	case 323: goto tr259;
	case 324: goto tr259;
	case 325: goto tr259;
	case 326: goto tr259;
	case 327: goto tr259;
	case 328: goto tr259;
	case 329: goto tr259;
	case 330: goto tr259;
	case 331: goto tr259;
	case 332: goto tr259;
	case 333: goto tr259;
	case 334: goto tr259;
	case 335: goto tr259;
	case 336: goto tr259;
	case 337: goto tr259;
	case 338: goto tr259;
	case 339: goto tr259;
	case 340: goto tr259;
	case 341: goto tr259;
	case 342: goto tr259;
	case 343: goto tr259;
	case 344: goto tr259;
	case 345: goto tr259;
	case 1715: goto tr2181;
	case 346: goto tr259;
	case 347: goto tr259;
	case 348: goto tr259;
	case 349: goto tr259;
	case 350: goto tr259;
	case 351: goto tr259;
	case 352: goto tr259;
	case 353: goto tr259;
	case 354: goto tr259;
	case 355: goto tr259;
	case 356: goto tr259;
	case 357: goto tr259;
	case 358: goto tr259;
	case 359: goto tr259;
	case 360: goto tr259;
	case 361: goto tr259;
	case 362: goto tr259;
	case 363: goto tr259;
	case 364: goto tr259;
	case 365: goto tr259;
	case 366: goto tr259;
	case 367: goto tr259;
	case 368: goto tr259;
	case 369: goto tr259;
	case 370: goto tr259;
	case 371: goto tr259;
	case 372: goto tr259;
	case 373: goto tr259;
	case 374: goto tr259;
	case 375: goto tr259;
	case 376: goto tr259;
	case 377: goto tr259;
	case 378: goto tr259;
	case 379: goto tr259;
	case 380: goto tr259;
	case 381: goto tr259;
	case 382: goto tr259;
	case 383: goto tr259;
	case 384: goto tr259;
	case 385: goto tr259;
	case 386: goto tr259;
	case 387: goto tr259;
	case 388: goto tr259;
	case 389: goto tr259;
	case 390: goto tr259;
	case 391: goto tr259;
	case 392: goto tr259;
	case 393: goto tr259;
	case 394: goto tr259;
	case 1716: goto tr2169;
	case 395: goto tr257;
	case 396: goto tr257;
	case 397: goto tr257;
	case 1717: goto tr2189;
	case 398: goto tr470;
	case 399: goto tr470;
	case 400: goto tr470;
	case 401: goto tr470;
	case 402: goto tr470;
	case 403: goto tr470;
	case 404: goto tr470;
	case 405: goto tr470;
	case 406: goto tr470;
	case 407: goto tr470;
	case 408: goto tr470;
	case 1718: goto tr2189;
	case 409: goto tr470;
	case 410: goto tr470;
	case 411: goto tr470;
	case 412: goto tr470;
	case 413: goto tr470;
	case 414: goto tr470;
	case 415: goto tr470;
	case 416: goto tr470;
	case 417: goto tr470;
	case 418: goto tr470;
	case 419: goto tr470;
	case 420: goto tr250;
	case 421: goto tr250;
	case 1719: goto tr2189;
	case 422: goto tr250;
	case 423: goto tr250;
	case 424: goto tr250;
	case 425: goto tr250;
	case 426: goto tr250;
	case 427: goto tr250;
	case 428: goto tr250;
	case 429: goto tr250;
	case 430: goto tr250;
	case 431: goto tr257;
	case 432: goto tr257;
	case 433: goto tr257;
	case 434: goto tr257;
	case 435: goto tr257;
	case 436: goto tr257;
	case 437: goto tr257;
	case 438: goto tr257;
	case 439: goto tr257;
	case 440: goto tr257;
	case 441: goto tr257;
	case 442: goto tr250;
	case 443: goto tr250;
	case 1720: goto tr2189;
	case 444: goto tr250;
	case 445: goto tr250;
	case 446: goto tr250;
	case 447: goto tr250;
	case 448: goto tr250;
	case 449: goto tr250;
	case 450: goto tr250;
	case 451: goto tr250;
	case 452: goto tr250;
	case 453: goto tr250;
	case 454: goto tr250;
	case 1721: goto tr2189;
	case 455: goto tr257;
	case 456: goto tr257;
	case 457: goto tr257;
	case 458: goto tr257;
	case 459: goto tr257;
	case 460: goto tr257;
	case 461: goto tr257;
	case 462: goto tr257;
	case 463: goto tr257;
	case 464: goto tr257;
	case 465: goto tr257;
	case 1722: goto tr2169;
	case 466: goto tr257;
	case 467: goto tr257;
	case 468: goto tr257;
	case 469: goto tr257;
	case 470: goto tr257;
	case 471: goto tr257;
	case 472: goto tr257;
	case 473: goto tr257;
	case 474: goto tr257;
	case 475: goto tr257;
	case 476: goto tr257;
	case 477: goto tr257;
	case 478: goto tr257;
	case 479: goto tr257;
	case 480: goto tr257;
	case 481: goto tr257;
	case 482: goto tr257;
	case 483: goto tr257;
	case 484: goto tr257;
	case 485: goto tr257;
	case 486: goto tr257;
	case 487: goto tr257;
	case 488: goto tr257;
	case 489: goto tr257;
	case 490: goto tr257;
	case 491: goto tr257;
	case 492: goto tr257;
	case 493: goto tr257;
	case 494: goto tr257;
	case 495: goto tr257;
	case 496: goto tr257;
	case 497: goto tr257;
	case 498: goto tr257;
	case 499: goto tr257;
	case 500: goto tr257;
	case 501: goto tr257;
	case 502: goto tr257;
	case 503: goto tr257;
	case 504: goto tr257;
	case 505: goto tr257;
	case 506: goto tr257;
	case 507: goto tr257;
	case 508: goto tr257;
	case 509: goto tr257;
	case 510: goto tr257;
	case 511: goto tr257;
	case 512: goto tr257;
	case 1723: goto tr2170;
	case 513: goto tr253;
	case 514: goto tr253;
	case 515: goto tr250;
	case 516: goto tr250;
	case 517: goto tr250;
	case 518: goto tr250;
	case 519: goto tr250;
	case 520: goto tr250;
	case 521: goto tr250;
	case 1724: goto tr2201;
	case 1725: goto tr2203;
	case 522: goto tr250;
	case 523: goto tr250;
	case 524: goto tr250;
	case 525: goto tr250;
	case 1726: goto tr2205;
	case 1727: goto tr2207;
	case 526: goto tr250;
	case 527: goto tr250;
	case 528: goto tr253;
	case 529: goto tr253;
	case 530: goto tr253;
	case 531: goto tr253;
	case 532: goto tr253;
	case 533: goto tr253;
	case 534: goto tr253;
	case 535: goto tr253;
	case 1728: goto tr2201;
	case 1729: goto tr2203;
	case 536: goto tr253;
	case 537: goto tr253;
	case 538: goto tr253;
	case 539: goto tr253;
	case 540: goto tr253;
	case 541: goto tr253;
	case 542: goto tr253;
	case 543: goto tr253;
	case 544: goto tr253;
	case 545: goto tr253;
	case 546: goto tr253;
	case 547: goto tr253;
	case 548: goto tr253;
	case 549: goto tr253;
	case 550: goto tr253;
	case 551: goto tr253;
	case 552: goto tr253;
	case 553: goto tr253;
	case 554: goto tr253;
	case 555: goto tr253;
	case 556: goto tr253;
	case 557: goto tr253;
	case 558: goto tr253;
	case 559: goto tr253;
	case 560: goto tr253;
	case 561: goto tr253;
	case 562: goto tr253;
	case 563: goto tr253;
	case 564: goto tr253;
	case 565: goto tr253;
	case 566: goto tr253;
	case 567: goto tr253;
	case 568: goto tr253;
	case 569: goto tr253;
	case 1730: goto tr2211;
	case 1731: goto tr2213;
	case 570: goto tr253;
	case 571: goto tr250;
	case 572: goto tr250;
	case 573: goto tr250;
	case 1732: goto tr2215;
	case 1733: goto tr2217;
	case 574: goto tr253;
	case 575: goto tr250;
	case 576: goto tr250;
	case 577: goto tr250;
	case 578: goto tr250;
	case 579: goto tr250;
	case 580: goto tr250;
	case 581: goto tr250;
	case 582: goto tr250;
	case 583: goto tr250;
	case 584: goto tr250;
	case 585: goto tr250;
	case 1734: goto tr2219;
	case 1735: goto tr2221;
	case 586: goto tr250;
	case 1736: goto tr2223;
	case 1737: goto tr2225;
	case 587: goto tr250;
	case 588: goto tr250;
	case 589: goto tr250;
	case 590: goto tr250;
	case 591: goto tr250;
	case 592: goto tr250;
	case 593: goto tr250;
	case 594: goto tr250;
	case 1738: goto tr2223;
	case 1739: goto tr2225;
	case 595: goto tr250;
	case 1740: goto tr2223;
	case 596: goto tr250;
	case 597: goto tr250;
	case 598: goto tr250;
	case 599: goto tr250;
	case 600: goto tr250;
	case 601: goto tr250;
	case 602: goto tr250;
	case 603: goto tr250;
	case 604: goto tr250;
	case 605: goto tr250;
	case 606: goto tr250;
	case 607: goto tr250;
	case 608: goto tr250;
	case 609: goto tr250;
	case 610: goto tr250;
	case 611: goto tr250;
	case 612: goto tr250;
	case 613: goto tr250;
	case 1741: goto tr2223;
	case 614: goto tr250;
	case 615: goto tr250;
	case 616: goto tr250;
	case 617: goto tr250;
	case 618: goto tr250;
	case 619: goto tr250;
	case 620: goto tr250;
	case 621: goto tr250;
	case 622: goto tr250;
	case 1742: goto tr2170;
	case 1743: goto tr2170;
	case 1744: goto tr2170;
	case 1745: goto tr2170;
	case 1746: goto tr2170;
	case 623: goto tr253;
	case 624: goto tr253;
	case 1747: goto tr2237;
	case 1748: goto tr2170;
	case 1749: goto tr2170;
	case 1750: goto tr2170;
	case 1751: goto tr2170;
	case 1752: goto tr2170;
	case 625: goto tr253;
	case 626: goto tr253;
	case 1753: goto tr2244;
	case 627: goto tr253;
	case 628: goto tr253;
	case 629: goto tr253;
	case 630: goto tr253;
	case 631: goto tr253;
	case 632: goto tr253;
	case 633: goto tr253;
	case 634: goto tr253;
	case 635: goto tr253;
	case 1754: goto tr2246;
	case 1755: goto tr2170;
	case 1756: goto tr2170;
	case 1757: goto tr2170;
	case 1758: goto tr2170;
	case 636: goto tr253;
	case 637: goto tr253;
	case 638: goto tr253;
	case 639: goto tr253;
	case 640: goto tr253;
	case 641: goto tr253;
	case 642: goto tr253;
	case 643: goto tr253;
	case 644: goto tr253;
	case 645: goto tr253;
	case 1759: goto tr2252;
	case 1760: goto tr2170;
	case 1761: goto tr2170;
	case 1762: goto tr2170;
	case 646: goto tr253;
	case 647: goto tr253;
	case 1763: goto tr2258;
	case 1764: goto tr2170;
	case 1765: goto tr2170;
	case 648: goto tr253;
	case 649: goto tr253;
	case 1766: goto tr2262;
	case 1767: goto tr2170;
	case 1768: goto tr2170;
	case 1769: goto tr2170;
	case 1770: goto tr2170;
	case 1771: goto tr2170;
	case 1772: goto tr2170;
	case 1773: goto tr2170;
	case 650: goto tr253;
	case 651: goto tr253;
	case 1774: goto tr2272;
	case 1775: goto tr2170;
	case 1776: goto tr2170;
	case 652: goto tr253;
	case 653: goto tr253;
	case 1777: goto tr2276;
	case 1778: goto tr2170;
	case 1779: goto tr2170;
	case 1780: goto tr2170;
	case 1781: goto tr2170;
	case 1782: goto tr2170;
	case 654: goto tr253;
	case 655: goto tr253;
	case 1783: goto tr2284;
	case 656: goto tr822;
	case 1784: goto tr2287;
	case 1785: goto tr2170;
	case 1786: goto tr2170;
	case 657: goto tr253;
	case 658: goto tr253;
	case 1787: goto tr2291;
	case 1788: goto tr2170;
	case 1789: goto tr2170;
	case 1790: goto tr2170;
	case 1791: goto tr2170;
	case 659: goto tr253;
	case 660: goto tr253;
	case 1792: goto tr2298;
	case 1793: goto tr2170;
	case 1794: goto tr2170;
	case 1795: goto tr2170;
	case 1796: goto tr2170;
	case 661: goto tr253;
	case 662: goto tr253;
	case 663: goto tr253;
	case 1797: goto tr2305;
	case 664: goto tr253;
	case 665: goto tr253;
	case 666: goto tr253;
	case 667: goto tr253;
	case 668: goto tr253;
	case 669: goto tr253;
	case 670: goto tr253;
	case 671: goto tr253;
	case 672: goto tr253;
	case 673: goto tr253;
	case 674: goto tr253;
	case 1798: goto tr2307;
	case 675: goto tr843;
	case 676: goto tr843;
	case 1799: goto tr2310;
	case 1800: goto tr2170;
	case 1801: goto tr2170;
	case 1802: goto tr2170;
	case 1803: goto tr2170;
	case 1804: goto tr2170;
	case 1805: goto tr2170;
	case 1806: goto tr2170;
	case 1807: goto tr2170;
	case 677: goto tr253;
	case 678: goto tr253;
	case 679: goto tr253;
	case 680: goto tr253;
	case 681: goto tr253;
	case 682: goto tr253;
	case 683: goto tr253;
	case 684: goto tr250;
	case 685: goto tr250;
	case 1808: goto tr2320;
	case 686: goto tr250;
	case 687: goto tr250;
	case 688: goto tr250;
	case 689: goto tr250;
	case 690: goto tr250;
	case 691: goto tr250;
	case 692: goto tr250;
	case 693: goto tr250;
	case 694: goto tr250;
	case 695: goto tr250;
	case 1809: goto tr2320;
	case 696: goto tr869;
	case 697: goto tr869;
	case 698: goto tr869;
	case 699: goto tr869;
	case 700: goto tr869;
	case 701: goto tr869;
	case 702: goto tr869;
	case 703: goto tr869;
	case 704: goto tr869;
	case 705: goto tr869;
	case 706: goto tr869;
	case 1810: goto tr2320;
	case 707: goto tr869;
	case 708: goto tr869;
	case 709: goto tr869;
	case 710: goto tr869;
	case 711: goto tr869;
	case 712: goto tr869;
	case 713: goto tr869;
	case 714: goto tr869;
	case 715: goto tr869;
	case 716: goto tr869;
	case 717: goto tr869;
	case 718: goto tr250;
	case 719: goto tr250;
	case 1811: goto tr2320;
	case 720: goto tr250;
	case 721: goto tr250;
	case 722: goto tr250;
	case 723: goto tr250;
	case 724: goto tr250;
	case 725: goto tr250;
	case 726: goto tr250;
	case 727: goto tr250;
	case 728: goto tr250;
	case 729: goto tr250;
	case 1812: goto tr2320;
	case 1813: goto tr2170;
	case 1814: goto tr2170;
	case 1815: goto tr2170;
	case 1816: goto tr2170;
	case 1817: goto tr2170;
	case 1818: goto tr2170;
	case 1819: goto tr2170;
	case 1820: goto tr2170;
	case 1821: goto tr2170;
	case 1822: goto tr2170;
	case 1823: goto tr2170;
	case 1824: goto tr2170;
	case 730: goto tr253;
	case 731: goto tr253;
	case 1825: goto tr2333;
	case 1826: goto tr2170;
	case 1827: goto tr2170;
	case 1828: goto tr2170;
	case 1829: goto tr2170;
	case 732: goto tr253;
	case 733: goto tr253;
	case 1830: goto tr2339;
	case 1831: goto tr2170;
	case 1832: goto tr2170;
	case 1833: goto tr2170;
	case 734: goto tr253;
	case 735: goto tr253;
	case 736: goto tr253;
	case 737: goto tr253;
	case 738: goto tr253;
	case 739: goto tr253;
	case 740: goto tr253;
	case 741: goto tr253;
	case 742: goto tr253;
	case 1834: goto tr2344;
	case 1835: goto tr2170;
	case 1836: goto tr2170;
	case 1837: goto tr2170;
	case 1838: goto tr2170;
	case 743: goto tr253;
	case 744: goto tr253;
	case 1839: goto tr2350;
	case 1840: goto tr2170;
	case 1841: goto tr2170;
	case 1842: goto tr2170;
	case 1843: goto tr2170;
	case 745: goto tr253;
	case 746: goto tr253;
	case 1844: goto tr2358;
	case 1845: goto tr2170;
	case 1846: goto tr2170;
	case 747: goto tr253;
	case 748: goto tr253;
	case 1847: goto tr2362;
	case 749: goto tr253;
	case 750: goto tr253;
	case 751: goto tr253;
	case 752: goto tr253;
	case 753: goto tr253;
	case 754: goto tr253;
	case 755: goto tr253;
	case 756: goto tr253;
	case 757: goto tr253;
	case 1848: goto tr2364;
	case 1849: goto tr2170;
	case 1850: goto tr2170;
	case 1851: goto tr2170;
	case 758: goto tr253;
	case 759: goto tr253;
	case 1852: goto tr2369;
	case 1853: goto tr2170;
	case 1854: goto tr2170;
	case 1855: goto tr2170;
	case 1856: goto tr2170;
	case 1857: goto tr2170;
	case 1858: goto tr2170;
	case 760: goto tr253;
	case 761: goto tr253;
	case 1859: goto tr2377;
	case 1860: goto tr2170;
	case 1861: goto tr2170;
	case 1862: goto tr2170;
	case 762: goto tr253;
	case 763: goto tr253;
	case 1863: goto tr2382;
	case 1864: goto tr2170;
	case 1865: goto tr2170;
	case 1866: goto tr2170;
	case 1867: goto tr2170;
	case 764: goto tr253;
	case 765: goto tr253;
	case 766: goto tr253;
	case 767: goto tr253;
	case 768: goto tr253;
	case 769: goto tr253;
	case 770: goto tr253;
	case 1868: goto tr2392;
	case 771: goto tr253;
	case 772: goto tr253;
	case 773: goto tr253;
	case 774: goto tr253;
	case 775: goto tr253;
	case 776: goto tr253;
	case 777: goto tr253;
	case 778: goto tr253;
	case 1869: goto tr2170;
	case 1870: goto tr2170;
	case 1871: goto tr2170;
	case 1872: goto tr2170;
	case 1873: goto tr2170;
	case 1874: goto tr2170;
	case 1875: goto tr2170;
	case 1876: goto tr2170;
	case 779: goto tr253;
	case 780: goto tr253;
	case 1877: goto tr2401;
	case 1878: goto tr2170;
	case 1879: goto tr2170;
	case 1880: goto tr2170;
	case 1881: goto tr2170;
	case 1882: goto tr2170;
	case 781: goto tr253;
	case 782: goto tr253;
	case 1883: goto tr2408;
	case 1884: goto tr2170;
	case 1885: goto tr2170;
	case 1886: goto tr2170;
	case 1887: goto tr2170;
	case 1888: goto tr2170;
	case 1889: goto tr2170;
	case 1890: goto tr2170;
	case 1891: goto tr2170;
	case 783: goto tr253;
	case 784: goto tr253;
	case 1892: goto tr2417;
	case 1893: goto tr2170;
	case 1894: goto tr2170;
	case 1895: goto tr2170;
	case 1896: goto tr2170;
	case 785: goto tr253;
	case 786: goto tr253;
	case 787: goto tr253;
	case 1897: goto tr2425;
	case 788: goto tr253;
	case 789: goto tr253;
	case 790: goto tr253;
	case 791: goto tr253;
	case 792: goto tr253;
	case 793: goto tr253;
	case 794: goto tr253;
	case 795: goto tr253;
	case 796: goto tr253;
	case 1898: goto tr2427;
	case 797: goto tr253;
	case 798: goto tr253;
	case 799: goto tr253;
	case 800: goto tr253;
	case 801: goto tr253;
	case 802: goto tr253;
	case 1899: goto tr2425;
	case 1900: goto tr2170;
	case 1901: goto tr2170;
	case 1902: goto tr2170;
	case 1903: goto tr2170;
	case 1904: goto tr2170;
	case 1905: goto tr2170;
	case 1906: goto tr2170;
	case 1907: goto tr2170;
	case 1908: goto tr2170;
	case 1909: goto tr2170;
	case 1910: goto tr2170;
	case 1911: goto tr2169;
	case 803: goto tr257;
	case 804: goto tr257;
	case 805: goto tr250;
	case 806: goto tr250;
	case 807: goto tr250;
	case 808: goto tr250;
	case 809: goto tr250;
	case 810: goto tr250;
	case 811: goto tr250;
	case 812: goto tr250;
	case 813: goto tr250;
	case 814: goto tr250;
	case 815: goto tr257;
	case 816: goto tr257;
	case 817: goto tr257;
	case 818: goto tr250;
	case 819: goto tr250;
	case 820: goto tr250;
	case 821: goto tr250;
	case 822: goto tr250;
	case 823: goto tr250;
	case 824: goto tr250;
	case 825: goto tr250;
	case 826: goto tr250;
	case 827: goto tr250;
	case 1912: goto tr250;
	case 828: goto tr250;
	case 829: goto tr257;
	case 1913: goto tr250;
	case 830: goto tr257;
	case 831: goto tr257;
	case 832: goto tr257;
	case 833: goto tr257;
	case 834: goto tr257;
	case 835: goto tr257;
	case 836: goto tr257;
	case 837: goto tr257;
	case 838: goto tr257;
	case 839: goto tr257;
	case 840: goto tr257;
	case 841: goto tr257;
	case 842: goto tr257;
	case 843: goto tr257;
	case 844: goto tr257;
	case 845: goto tr257;
	case 846: goto tr257;
	case 847: goto tr257;
	case 848: goto tr257;
	case 849: goto tr257;
	case 850: goto tr257;
	case 851: goto tr257;
	case 852: goto tr257;
	case 853: goto tr257;
	case 854: goto tr257;
	case 855: goto tr257;
	case 856: goto tr257;
	case 857: goto tr257;
	case 858: goto tr257;
	case 859: goto tr257;
	case 860: goto tr257;
	case 861: goto tr257;
	case 862: goto tr257;
	case 863: goto tr257;
	case 864: goto tr257;
	case 865: goto tr257;
	case 866: goto tr257;
	case 867: goto tr257;
	case 868: goto tr257;
	case 869: goto tr257;
	case 870: goto tr257;
	case 871: goto tr257;
	case 872: goto tr257;
	case 873: goto tr257;
	case 874: goto tr257;
	case 875: goto tr257;
	case 876: goto tr257;
	case 877: goto tr257;
	case 878: goto tr257;
	case 879: goto tr257;
	case 880: goto tr257;
	case 881: goto tr257;
	case 882: goto tr257;
	case 883: goto tr257;
	case 884: goto tr257;
	case 885: goto tr257;
	case 886: goto tr257;
	case 887: goto tr257;
	case 888: goto tr257;
	case 889: goto tr257;
	case 890: goto tr257;
	case 891: goto tr257;
	case 892: goto tr257;
	case 893: goto tr257;
	case 894: goto tr257;
	case 1914: goto tr2453;
	case 895: goto tr1090;
	case 1915: goto tr2454;
	case 896: goto tr1093;
	case 897: goto tr257;
	case 898: goto tr257;
	case 899: goto tr257;
	case 900: goto tr257;
	case 901: goto tr257;
	case 902: goto tr257;
	case 903: goto tr257;
	case 904: goto tr257;
	case 905: goto tr257;
	case 906: goto tr257;
	case 907: goto tr257;
	case 908: goto tr257;
	case 909: goto tr257;
	case 910: goto tr257;
	case 911: goto tr257;
	case 912: goto tr257;
	case 1916: goto tr250;
	case 913: goto tr257;
	case 914: goto tr257;
	case 915: goto tr257;
	case 916: goto tr257;
	case 917: goto tr257;
	case 918: goto tr257;
	case 919: goto tr257;
	case 920: goto tr257;
	case 921: goto tr257;
	case 922: goto tr257;
	case 923: goto tr257;
	case 924: goto tr257;
	case 925: goto tr257;
	case 926: goto tr257;
	case 927: goto tr257;
	case 928: goto tr257;
	case 929: goto tr257;
	case 930: goto tr257;
	case 931: goto tr257;
	case 932: goto tr257;
	case 933: goto tr257;
	case 934: goto tr257;
	case 935: goto tr257;
	case 936: goto tr257;
	case 937: goto tr257;
	case 938: goto tr257;
	case 939: goto tr257;
	case 940: goto tr257;
	case 941: goto tr257;
	case 942: goto tr257;
	case 943: goto tr257;
	case 944: goto tr257;
	case 945: goto tr257;
	case 946: goto tr257;
	case 947: goto tr257;
	case 948: goto tr257;
	case 949: goto tr257;
	case 950: goto tr257;
	case 951: goto tr257;
	case 952: goto tr257;
	case 953: goto tr257;
	case 954: goto tr257;
	case 955: goto tr257;
	case 956: goto tr257;
	case 957: goto tr257;
	case 958: goto tr257;
	case 959: goto tr257;
	case 960: goto tr257;
	case 961: goto tr257;
	case 962: goto tr257;
	case 963: goto tr257;
	case 964: goto tr257;
	case 965: goto tr257;
	case 966: goto tr257;
	case 967: goto tr257;
	case 968: goto tr257;
	case 969: goto tr257;
	case 970: goto tr257;
	case 971: goto tr257;
	case 972: goto tr257;
	case 973: goto tr257;
	case 974: goto tr257;
	case 975: goto tr257;
	case 976: goto tr257;
	case 977: goto tr257;
	case 978: goto tr257;
	case 979: goto tr257;
	case 980: goto tr257;
	case 981: goto tr257;
	case 982: goto tr257;
	case 983: goto tr257;
	case 984: goto tr257;
	case 985: goto tr257;
	case 986: goto tr257;
	case 987: goto tr257;
	case 988: goto tr257;
	case 989: goto tr257;
	case 990: goto tr257;
	case 991: goto tr257;
	case 992: goto tr257;
	case 993: goto tr257;
	case 994: goto tr257;
	case 995: goto tr257;
	case 996: goto tr257;
	case 997: goto tr257;
	case 998: goto tr257;
	case 999: goto tr257;
	case 1000: goto tr257;
	case 1001: goto tr257;
	case 1002: goto tr257;
	case 1003: goto tr257;
	case 1004: goto tr257;
	case 1005: goto tr257;
	case 1006: goto tr257;
	case 1007: goto tr257;
	case 1008: goto tr257;
	case 1009: goto tr257;
	case 1010: goto tr257;
	case 1011: goto tr257;
	case 1012: goto tr257;
	case 1013: goto tr257;
	case 1014: goto tr257;
	case 1015: goto tr257;
	case 1016: goto tr257;
	case 1017: goto tr257;
	case 1018: goto tr257;
	case 1019: goto tr257;
	case 1020: goto tr257;
	case 1021: goto tr257;
	case 1022: goto tr257;
	case 1023: goto tr257;
	case 1024: goto tr257;
	case 1025: goto tr257;
	case 1026: goto tr257;
	case 1027: goto tr257;
	case 1028: goto tr257;
	case 1029: goto tr257;
	case 1030: goto tr257;
	case 1031: goto tr257;
	case 1032: goto tr257;
	case 1033: goto tr257;
	case 1034: goto tr257;
	case 1035: goto tr257;
	case 1036: goto tr257;
	case 1037: goto tr257;
	case 1038: goto tr257;
	case 1039: goto tr257;
	case 1040: goto tr257;
	case 1041: goto tr257;
	case 1042: goto tr257;
	case 1043: goto tr257;
	case 1044: goto tr257;
	case 1045: goto tr257;
	case 1046: goto tr257;
	case 1047: goto tr257;
	case 1048: goto tr257;
	case 1049: goto tr257;
	case 1050: goto tr257;
	case 1051: goto tr257;
	case 1052: goto tr257;
	case 1053: goto tr257;
	case 1054: goto tr257;
	case 1055: goto tr257;
	case 1056: goto tr257;
	case 1057: goto tr257;
	case 1058: goto tr257;
	case 1059: goto tr257;
	case 1060: goto tr257;
	case 1061: goto tr257;
	case 1062: goto tr257;
	case 1063: goto tr257;
	case 1064: goto tr257;
	case 1065: goto tr257;
	case 1066: goto tr257;
	case 1067: goto tr257;
	case 1068: goto tr250;
	case 1069: goto tr250;
	case 1070: goto tr250;
	case 1071: goto tr257;
	case 1072: goto tr257;
	case 1073: goto tr257;
	case 1917: goto tr2455;
	case 1074: goto tr257;
	case 1075: goto tr257;
	case 1918: goto tr2455;
	case 1076: goto tr257;
	case 1077: goto tr257;
	case 1078: goto tr257;
	case 1079: goto tr257;
	case 1080: goto tr257;
	case 1081: goto tr257;
	case 1082: goto tr257;
	case 1083: goto tr257;
	case 1084: goto tr257;
	case 1085: goto tr257;
	case 1086: goto tr257;
	case 1087: goto tr257;
	case 1088: goto tr257;
	case 1089: goto tr257;
	case 1090: goto tr257;
	case 1091: goto tr257;
	case 1092: goto tr257;
	case 1919: goto tr2456;
	case 1093: goto tr1296;
	case 1094: goto tr257;
	case 1095: goto tr257;
	case 1096: goto tr257;
	case 1097: goto tr257;
	case 1098: goto tr257;
	case 1099: goto tr257;
	case 1100: goto tr257;
	case 1101: goto tr257;
	case 1102: goto tr257;
	case 1103: goto tr257;
	case 1104: goto tr257;
	case 1105: goto tr257;
	case 1106: goto tr257;
	case 1107: goto tr257;
	case 1108: goto tr257;
	case 1109: goto tr257;
	case 1110: goto tr257;
	case 1111: goto tr257;
	case 1112: goto tr257;
	case 1113: goto tr257;
	case 1114: goto tr257;
	case 1115: goto tr257;
	case 1116: goto tr257;
	case 1117: goto tr257;
	case 1118: goto tr257;
	case 1119: goto tr257;
	case 1120: goto tr257;
	case 1121: goto tr257;
	case 1122: goto tr257;
	case 1123: goto tr257;
	case 1124: goto tr257;
	case 1125: goto tr257;
	case 1126: goto tr257;
	case 1127: goto tr257;
	case 1128: goto tr257;
	case 1129: goto tr250;
	case 1130: goto tr250;
	case 1131: goto tr250;
	case 1132: goto tr250;
	case 1133: goto tr250;
	case 1134: goto tr250;
	case 1135: goto tr250;
	case 1136: goto tr250;
	case 1137: goto tr257;
	case 1138: goto tr257;
	case 1920: goto tr2455;
	case 1139: goto tr257;
	case 1140: goto tr257;
	case 1141: goto tr257;
	case 1142: goto tr257;
	case 1143: goto tr257;
	case 1144: goto tr257;
	case 1145: goto tr257;
	case 1146: goto tr257;
	case 1147: goto tr257;
	case 1148: goto tr257;
	case 1149: goto tr257;
	case 1150: goto tr257;
	case 1151: goto tr257;
	case 1152: goto tr257;
	case 1153: goto tr250;
	case 1154: goto tr250;
	case 1155: goto tr257;
	case 1156: goto tr257;
	case 1157: goto tr257;
	case 1158: goto tr257;
	case 1921: goto tr2455;
	case 1159: goto tr257;
	case 1160: goto tr257;
	case 1161: goto tr257;
	case 1162: goto tr257;
	case 1163: goto tr257;
	case 1164: goto tr257;
	case 1165: goto tr257;
	case 1166: goto tr257;
	case 1167: goto tr257;
	case 1168: goto tr257;
	case 1169: goto tr257;
	case 1170: goto tr257;
	case 1171: goto tr257;
	case 1172: goto tr257;
	case 1173: goto tr257;
	case 1174: goto tr257;
	case 1175: goto tr257;
	case 1176: goto tr257;
	case 1177: goto tr257;
	case 1178: goto tr257;
	case 1179: goto tr250;
	case 1180: goto tr257;
	case 1181: goto tr257;
	case 1922: goto tr2455;
	case 1182: goto tr257;
	case 1183: goto tr257;
	case 1184: goto tr257;
	case 1185: goto tr257;
	case 1186: goto tr257;
	case 1187: goto tr257;
	case 1188: goto tr257;
	case 1189: goto tr257;
	case 1190: goto tr257;
	case 1191: goto tr257;
	case 1192: goto tr257;
	case 1193: goto tr257;
	case 1194: goto tr257;
	case 1195: goto tr257;
	case 1196: goto tr257;
	case 1197: goto tr257;
	case 1198: goto tr257;
	case 1199: goto tr257;
	case 1200: goto tr257;
	case 1201: goto tr257;
	case 1202: goto tr257;
	case 1203: goto tr257;
	case 1204: goto tr257;
	case 1205: goto tr257;
	case 1206: goto tr250;
	case 1207: goto tr257;
	case 1208: goto tr257;
	case 1923: goto tr2455;
	case 1209: goto tr257;
	case 1210: goto tr257;
	case 1211: goto tr257;
	case 1212: goto tr257;
	case 1213: goto tr257;
	case 1214: goto tr257;
	case 1215: goto tr257;
	case 1216: goto tr257;
	case 1217: goto tr257;
	case 1218: goto tr257;
	case 1219: goto tr257;
	case 1220: goto tr257;
	case 1221: goto tr257;
	case 1222: goto tr257;
	case 1223: goto tr257;
	case 1224: goto tr257;
	case 1225: goto tr257;
	case 1226: goto tr257;
	case 1227: goto tr257;
	case 1228: goto tr257;
	case 1229: goto tr257;
	case 1230: goto tr257;
	case 1231: goto tr257;
	case 1232: goto tr257;
	case 1233: goto tr257;
	case 1234: goto tr257;
	case 1235: goto tr257;
	case 1236: goto tr257;
	case 1237: goto tr257;
	case 1238: goto tr257;
	case 1239: goto tr257;
	case 1240: goto tr257;
	case 1241: goto tr257;
	case 1242: goto tr257;
	case 1243: goto tr257;
	case 1244: goto tr257;
	case 1245: goto tr257;
	case 1246: goto tr257;
	case 1247: goto tr257;
	case 1248: goto tr257;
	case 1249: goto tr257;
	case 1250: goto tr257;
	case 1251: goto tr257;
	case 1252: goto tr257;
	case 1253: goto tr257;
	case 1254: goto tr257;
	case 1255: goto tr257;
	case 1256: goto tr257;
	case 1257: goto tr257;
	case 1258: goto tr257;
	case 1259: goto tr257;
	case 1260: goto tr257;
	case 1261: goto tr257;
	case 1262: goto tr257;
	case 1263: goto tr257;
	case 1264: goto tr257;
	case 1265: goto tr257;
	case 1266: goto tr257;
	case 1267: goto tr257;
	case 1268: goto tr257;
	case 1269: goto tr257;
	case 1270: goto tr257;
	case 1271: goto tr257;
	case 1272: goto tr257;
	case 1273: goto tr257;
	case 1274: goto tr257;
	case 1275: goto tr257;
	case 1276: goto tr257;
	case 1277: goto tr257;
	case 1278: goto tr257;
	case 1279: goto tr257;
	case 1280: goto tr257;
	case 1281: goto tr257;
	case 1282: goto tr257;
	case 1283: goto tr257;
	case 1284: goto tr257;
	case 1285: goto tr257;
	case 1286: goto tr257;
	case 1287: goto tr257;
	case 1288: goto tr257;
	case 1289: goto tr257;
	case 1290: goto tr257;
	case 1291: goto tr257;
	case 1292: goto tr257;
	case 1293: goto tr257;
	case 1294: goto tr257;
	case 1295: goto tr257;
	case 1296: goto tr257;
	case 1297: goto tr257;
	case 1298: goto tr257;
	case 1924: goto tr2169;
	case 1925: goto tr2169;
	case 1299: goto tr257;
	case 1300: goto tr257;
	case 1301: goto tr257;
	case 1302: goto tr257;
	case 1303: goto tr257;
	case 1304: goto tr257;
	case 1305: goto tr257;
	case 1306: goto tr257;
	case 1307: goto tr257;
	case 1308: goto tr257;
	case 1309: goto tr257;
	case 1310: goto tr257;
	case 1311: goto tr257;
	case 1312: goto tr257;
	case 1313: goto tr257;
	case 1314: goto tr257;
	case 1315: goto tr257;
	case 1316: goto tr257;
	case 1317: goto tr257;
	case 1318: goto tr257;
	case 1319: goto tr257;
	case 1320: goto tr257;
	case 1321: goto tr257;
	case 1322: goto tr257;
	case 1323: goto tr257;
	case 1324: goto tr257;
	case 1325: goto tr257;
	case 1326: goto tr257;
	case 1327: goto tr257;
	case 1328: goto tr257;
	case 1329: goto tr257;
	case 1330: goto tr257;
	case 1331: goto tr257;
	case 1332: goto tr257;
	case 1333: goto tr257;
	case 1334: goto tr257;
	case 1335: goto tr257;
	case 1336: goto tr257;
	case 1337: goto tr257;
	case 1338: goto tr257;
	case 1339: goto tr257;
	case 1340: goto tr257;
	case 1341: goto tr257;
	case 1342: goto tr257;
	case 1343: goto tr257;
	case 1344: goto tr257;
	case 1926: goto tr2453;
	case 1345: goto tr257;
	case 1346: goto tr257;
	case 1347: goto tr257;
	case 1348: goto tr257;
	case 1349: goto tr257;
	case 1350: goto tr257;
	case 1351: goto tr257;
	case 1352: goto tr257;
	case 1353: goto tr257;
	case 1354: goto tr257;
	case 1355: goto tr257;
	case 1356: goto tr257;
	case 1357: goto tr257;
	case 1358: goto tr257;
	case 1359: goto tr257;
	case 1360: goto tr257;
	case 1361: goto tr257;
	case 1362: goto tr257;
	case 1363: goto tr257;
	case 1364: goto tr257;
	case 1365: goto tr257;
	case 1366: goto tr257;
	case 1367: goto tr257;
	case 1368: goto tr257;
	case 1369: goto tr257;
	case 1370: goto tr257;
	case 1371: goto tr257;
	case 1372: goto tr257;
	case 1373: goto tr257;
	case 1374: goto tr257;
	case 1375: goto tr257;
	case 1376: goto tr257;
	case 1377: goto tr257;
	case 1378: goto tr257;
	case 1379: goto tr257;
	case 1380: goto tr257;
	case 1381: goto tr257;
	case 1927: goto tr2169;
	case 1382: goto tr257;
	case 1383: goto tr257;
	case 1928: goto tr2169;
	case 1384: goto tr257;
	case 1385: goto tr250;
	case 1386: goto tr250;
	case 1387: goto tr250;
	case 1929: goto tr2476;
	case 1388: goto tr250;
	case 1389: goto tr250;
	case 1390: goto tr250;
	case 1391: goto tr250;
	case 1392: goto tr250;
	case 1393: goto tr250;
	case 1394: goto tr250;
	case 1395: goto tr250;
	case 1396: goto tr250;
	case 1397: goto tr250;
	case 1930: goto tr2476;
	case 1398: goto tr250;
	case 1399: goto tr250;
	case 1400: goto tr250;
	case 1401: goto tr250;
	case 1402: goto tr250;
	case 1403: goto tr250;
	case 1404: goto tr250;
	case 1405: goto tr250;
	case 1406: goto tr250;
	case 1407: goto tr250;
	case 1408: goto tr257;
	case 1409: goto tr257;
	case 1410: goto tr257;
	case 1411: goto tr257;
	case 1412: goto tr257;
	case 1413: goto tr257;
	case 1414: goto tr257;
	case 1415: goto tr257;
	case 1416: goto tr257;
	case 1417: goto tr257;
	case 1932: goto tr2482;
	case 1418: goto tr1695;
	case 1419: goto tr1695;
	case 1420: goto tr1695;
	case 1421: goto tr1695;
	case 1422: goto tr1695;
	case 1423: goto tr1695;
	case 1424: goto tr1695;
	case 1425: goto tr1695;
	case 1426: goto tr1695;
	case 1427: goto tr1695;
	case 1428: goto tr1695;
	case 1429: goto tr1695;
	case 1933: goto tr2482;
	case 1934: goto tr2482;
	case 1936: goto tr2490;
	case 1430: goto tr1707;
	case 1431: goto tr1707;
	case 1432: goto tr1707;
	case 1433: goto tr1707;
	case 1434: goto tr1707;
	case 1435: goto tr1707;
	case 1436: goto tr1707;
	case 1437: goto tr1707;
	case 1438: goto tr1707;
	case 1439: goto tr1707;
	case 1440: goto tr1707;
	case 1441: goto tr1707;
	case 1442: goto tr1707;
	case 1443: goto tr1707;
	case 1444: goto tr1707;
	case 1445: goto tr1707;
	case 1446: goto tr1707;
	case 1447: goto tr1707;
	case 1937: goto tr2490;
	case 1938: goto tr2490;
	case 1940: goto tr2496;
	case 1448: goto tr1725;
	case 1449: goto tr1725;
	case 1450: goto tr1725;
	case 1451: goto tr1725;
	case 1452: goto tr1725;
	case 1453: goto tr1725;
	case 1454: goto tr1725;
	case 1455: goto tr1725;
	case 1456: goto tr1725;
	case 1457: goto tr1725;
	case 1458: goto tr1725;
	case 1459: goto tr1725;
	case 1460: goto tr1725;
	case 1461: goto tr1725;
	case 1462: goto tr1725;
	case 1463: goto tr1725;
	case 1464: goto tr1725;
	case 1465: goto tr1725;
	case 1466: goto tr1725;
	case 1467: goto tr1725;
	case 1468: goto tr1725;
	case 1469: goto tr1725;
	case 1470: goto tr1725;
	case 1471: goto tr1725;
	case 1472: goto tr1725;
	case 1473: goto tr1725;
	case 1474: goto tr1725;
	case 1475: goto tr1725;
	case 1476: goto tr1725;
	case 1477: goto tr1725;
	case 1478: goto tr1725;
	case 1479: goto tr1725;
	case 1480: goto tr1725;
	case 1481: goto tr1725;
	case 1482: goto tr1725;
	case 1483: goto tr1725;
	case 1484: goto tr1725;
	case 1485: goto tr1725;
	case 1486: goto tr1725;
	case 1487: goto tr1725;
	case 1488: goto tr1725;
	case 1489: goto tr1725;
	case 1490: goto tr1725;
	case 1491: goto tr1725;
	case 1492: goto tr1725;
	case 1493: goto tr1725;
	case 1494: goto tr1725;
	case 1495: goto tr1725;
	case 1496: goto tr1725;
	case 1497: goto tr1725;
	case 1498: goto tr1725;
	case 1499: goto tr1725;
	case 1500: goto tr1725;
	case 1501: goto tr1725;
	case 1502: goto tr1725;
	case 1503: goto tr1725;
	case 1504: goto tr1725;
	case 1505: goto tr1725;
	case 1506: goto tr1725;
	case 1507: goto tr1725;
	case 1508: goto tr1725;
	case 1509: goto tr1725;
	case 1510: goto tr1725;
	case 1511: goto tr1725;
	case 1512: goto tr1725;
	case 1513: goto tr1725;
	case 1514: goto tr1725;
	case 1515: goto tr1725;
	case 1516: goto tr1725;
	case 1517: goto tr1725;
	case 1518: goto tr1725;
	case 1519: goto tr1725;
	case 1520: goto tr1725;
	case 1521: goto tr1725;
	case 1522: goto tr1725;
	case 1523: goto tr1725;
	case 1524: goto tr1725;
	case 1525: goto tr1725;
	case 1526: goto tr1725;
	case 1527: goto tr1725;
	case 1528: goto tr1725;
	case 1529: goto tr1725;
	case 1530: goto tr1725;
	case 1531: goto tr1725;
	case 1532: goto tr1725;
	case 1533: goto tr1725;
	case 1534: goto tr1725;
	case 1535: goto tr1725;
	case 1536: goto tr1725;
	case 1537: goto tr1725;
	case 1538: goto tr1725;
	case 1539: goto tr1725;
	case 1540: goto tr1725;
	case 1541: goto tr1725;
	case 1542: goto tr1725;
	case 1543: goto tr1725;
	case 1544: goto tr1725;
	case 1545: goto tr1725;
	case 1546: goto tr1725;
	case 1547: goto tr1725;
	case 1548: goto tr1725;
	case 1549: goto tr1725;
	case 1550: goto tr1725;
	case 1551: goto tr1725;
	case 1552: goto tr1725;
	case 1553: goto tr1725;
	case 1554: goto tr1725;
	case 1555: goto tr1725;
	case 1556: goto tr1725;
	case 1557: goto tr1725;
	case 1558: goto tr1725;
	case 1559: goto tr1725;
	case 1941: goto tr2496;
	case 1560: goto tr1725;
	case 1561: goto tr1725;
	case 1562: goto tr1725;
	case 1563: goto tr1725;
	case 1564: goto tr1725;
	case 1565: goto tr1725;
	case 1566: goto tr1725;
	case 1567: goto tr1725;
	case 1568: goto tr1725;
	case 1569: goto tr1725;
	case 1570: goto tr1725;
	case 1571: goto tr1725;
	case 1572: goto tr1725;
	case 1573: goto tr1725;
	case 1574: goto tr1725;
	case 1575: goto tr1725;
	case 1576: goto tr1725;
	case 1577: goto tr1725;
	case 1578: goto tr1725;
	case 1579: goto tr1725;
	case 1580: goto tr1725;
	case 1581: goto tr1725;
	case 1582: goto tr1725;
	case 1583: goto tr1725;
	case 1584: goto tr1725;
	case 1585: goto tr1725;
	case 1586: goto tr1725;
	case 1587: goto tr1725;
	case 1588: goto tr1725;
	case 1589: goto tr1725;
	case 1590: goto tr1725;
	case 1591: goto tr1725;
	case 1592: goto tr1725;
	case 1593: goto tr1725;
	case 1594: goto tr1725;
	case 1595: goto tr1725;
	case 1596: goto tr1725;
	case 1597: goto tr1725;
	case 1598: goto tr1725;
	case 1599: goto tr1725;
	case 1600: goto tr1725;
	case 1601: goto tr1725;
	case 1602: goto tr1725;
	case 1603: goto tr1725;
	case 1604: goto tr1725;
	case 1605: goto tr1725;
	case 1606: goto tr1725;
	case 1607: goto tr1725;
	case 1608: goto tr1725;
	case 1609: goto tr1725;
	case 1610: goto tr1725;
	case 1611: goto tr1725;
	case 1612: goto tr1725;
	case 1613: goto tr1725;
	case 1614: goto tr1725;
	case 1615: goto tr1725;
	case 1616: goto tr1725;
	case 1617: goto tr1725;
	case 1618: goto tr1725;
	case 1619: goto tr1725;
	case 1620: goto tr1725;
	case 1621: goto tr1725;
	case 1622: goto tr1725;
	case 1623: goto tr1725;
	case 1624: goto tr1725;
	case 1625: goto tr1725;
	case 1626: goto tr1725;
	case 1627: goto tr1725;
	case 1628: goto tr1725;
	case 1629: goto tr1725;
	case 1630: goto tr1725;
	case 1631: goto tr1725;
	case 1632: goto tr1725;
	case 1633: goto tr1725;
	case 1634: goto tr1725;
	case 1635: goto tr1725;
	case 1636: goto tr1725;
	case 1637: goto tr1725;
	case 1638: goto tr1725;
	case 1639: goto tr1725;
	case 1640: goto tr1725;
	case 1641: goto tr1725;
	case 1642: goto tr1725;
	case 1643: goto tr1725;
	case 1644: goto tr1725;
	case 1645: goto tr1725;
	case 1646: goto tr1725;
	case 1647: goto tr1725;
	case 1648: goto tr1725;
	case 1649: goto tr1725;
	case 1650: goto tr1725;
	case 1651: goto tr1725;
	case 1652: goto tr1725;
	case 1653: goto tr1725;
	case 1654: goto tr1725;
	case 1655: goto tr1725;
	case 1656: goto tr1725;
	case 1657: goto tr1725;
	case 1658: goto tr1725;
	case 1659: goto tr1725;
	case 1660: goto tr1725;
	case 1661: goto tr1725;
	case 1662: goto tr1725;
	case 1663: goto tr1725;
	case 1664: goto tr1725;
	case 1665: goto tr1725;
	case 1666: goto tr1725;
	case 1667: goto tr1725;
	case 1668: goto tr1725;
	case 1669: goto tr1725;
	case 1670: goto tr1725;
	case 1671: goto tr1725;
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
