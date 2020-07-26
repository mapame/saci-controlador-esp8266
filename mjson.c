// Copyright (c) 2018 Cesanta Software Limited
// All rights reserved
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mjson.h"

static int mjson_esc(int c, int esc) {
  const char *p, *esc1 = "\b\f\n\r\t\\\"", *esc2 = "bfnrt\\\"";
  for (p = esc ? esc1 : esc2; *p != '\0'; p++) {
    if (*p == c) return esc ? esc2[p - esc1] : esc1[p - esc2];
  }
  return 0;
}

static int mjson_pass_string(const char *s, int len) {
  int i;
  for (i = 0; i < len; i++) {
    if (s[i] == '\\' && i + 1 < len && mjson_esc(s[i + 1], 1)) {
      i++;
    } else if (s[i] == '\0') {
      return MJSON_ERROR_INVALID_INPUT;
    } else if (s[i] == '"') {
      return i;
    }
  }
  return MJSON_ERROR_INVALID_INPUT;
}

int ATTR mjson(const char *s, int len, mjson_cb_t cb, void *ud) {
  enum { S_VALUE, S_KEY, S_COLON, S_COMMA_OR_EOO } expecting = S_VALUE;
  unsigned char nesting[MJSON_MAX_DEPTH];
  int i, depth = 0;
#define MJSONCALL(ev) \
  if (cb != NULL && cb(ev, s, start, i - start + 1, ud)) return i + 1;

// In the ascii table, the distance between `[` and `]` is 2.
// Ditto for `{` and `}`. Hence +2 in the code below.
#define MJSONEOO()                                                     \
  do {                                                                 \
    if (c != nesting[depth - 1] + 2) return MJSON_ERROR_INVALID_INPUT; \
    depth--;                                                           \
    if (depth == 0) {                                                  \
      MJSONCALL(tok);                                                  \
      return i + 1;                                                    \
    }                                                                  \
  } while (0)

  for (i = 0; i < len; i++) {
    int start = i;
    unsigned char c = ((unsigned char *) s)[i];
    int tok = c;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
    // printf("- %c [%.*s] %d %d\n", c, i, s, depth, expecting);
    switch (expecting) {
      case S_VALUE:
        if (c == '{') {
          if (depth >= (int) sizeof(nesting)) return MJSON_ERROR_TOO_DEEP;
          nesting[depth++] = c;
          expecting = S_KEY;
          break;
        } else if (c == '[') {
          if (depth >= (int) sizeof(nesting)) return MJSON_ERROR_TOO_DEEP;
          nesting[depth++] = c;
          break;
        } else if (c == ']' && depth > 0) {  // Empty array
          MJSONEOO();
        } else if (c == 't' && i + 3 < len && memcmp(&s[i], "true", 4) == 0) {
          i += 3;
          tok = MJSON_TOK_TRUE;
        } else if (c == 'n' && i + 3 < len && memcmp(&s[i], "null", 4) == 0) {
          i += 3;
          tok = MJSON_TOK_NULL;
        } else if (c == 'f' && i + 4 < len && memcmp(&s[i], "false", 5) == 0) {
          i += 4;
          tok = MJSON_TOK_FALSE;
        } else if (c == '-' || ((c >= '0' && c <= '9'))) {
          char *end = NULL;
          strtod(&s[i], &end);
          if (end != NULL) i += end - &s[i] - 1;
          tok = MJSON_TOK_NUMBER;
        } else if (c == '"') {
          int n = mjson_pass_string(&s[i + 1], len - i - 1);
          if (n < 0) return n;
          i += n + 1;
          tok = MJSON_TOK_STRING;
        } else {
          return MJSON_ERROR_INVALID_INPUT;
        }
        if (depth == 0) {
          MJSONCALL(tok);
          return i + 1;
        }
        expecting = S_COMMA_OR_EOO;
        break;

      case S_KEY:
        if (c == '"') {
          int n = mjson_pass_string(&s[i + 1], len - i - 1);
          if (n < 0) return n;
          i += n + 1;
          tok = MJSON_TOK_KEY;
          expecting = S_COLON;
        } else if (c == '}') {  // Empty object
          MJSONEOO();
          expecting = S_COMMA_OR_EOO;
        } else {
          return MJSON_ERROR_INVALID_INPUT;
        }
        break;

      case S_COLON:
        if (c == ':') {
          expecting = S_VALUE;
        } else {
          return MJSON_ERROR_INVALID_INPUT;
        }
        break;

      case S_COMMA_OR_EOO:
        if (depth <= 0) return MJSON_ERROR_INVALID_INPUT;
        if (c == ',') {
          expecting = (nesting[depth - 1] == '{') ? S_KEY : S_VALUE;
        } else if (c == ']' || c == '}') {
          MJSONEOO();
        } else {
          return MJSON_ERROR_INVALID_INPUT;
        }
        break;
    }
    MJSONCALL(tok);
  }
  return MJSON_ERROR_INVALID_INPUT;
}

struct msjon_get_data {
  const char *path;     // Lookup json path
  int pos;              // Current path index
  int d1;               // Current depth of traversal
  int d2;               // Expected depth of traversal
  int i1;               // Index in an array
  int i2;               // Expected index in an array
  int obj;              // If the value is array/object, offset where it starts
  const char **tokptr;  // Destination
  int *toklen;          // Destination length
  int tok;              // Returned token
};

static int mjson_plen(const char *s) {
  int i = 0;
  while (s[i] != '\0' && s[i] != '.' && s[i] != '[') i++;
  return i;
}

static int ATTR mjson_get_cb(int tok, const char *s, int off, int len,
                             void *ud) {
  struct msjon_get_data *data = (struct msjon_get_data *) ud;
  // printf("--> %2x %2d %2d %2d %2d\t'%s'\t'%.*s'\t\t'%.*s'\n", tok, data->d1,
  // data->d2, data->i1, data->i2, data->path + data->pos, off, s, len,
  // s + off);
  if (data->tok != MJSON_TOK_INVALID) return 1;  // Found

  if (tok == '{') {
    if (!data->path[data->pos] && data->d1 == data->d2) data->obj = off;
    data->d1++;
  } else if (tok == '[') {
    if (data->d1 == data->d2 && data->path[data->pos] == '[') {
      data->i1 = 0;
      data->i2 = (int) strtod(&data->path[data->pos + 1], NULL);
      if (data->i1 == data->i2) {
        data->d2++;
        data->pos += 3;
      }
    }
    if (!data->path[data->pos] && data->d1 == data->d2) data->obj = off;
    data->d1++;
  } else if (tok == ',') {
    if (data->d1 == data->d2 + 1) {
      data->i1++;
      if (data->i1 == data->i2) {
        while (data->path[data->pos] != ']') data->pos++;
        data->pos++;
        data->d2++;
      }
    }
  } else if (tok == MJSON_TOK_KEY && data->d1 == data->d2 + 1 &&
             data->path[data->pos] == '.' && s[off] == '"' &&
             s[off + len - 1] == '"' &&
             mjson_plen(&data->path[data->pos + 1]) == len - 2 &&
             !memcmp(s + off + 1, &data->path[data->pos + 1], len - 2)) {
    data->d2++;
    data->pos += len - 1;
  } else if (tok == MJSON_TOK_KEY && data->d1 == data->d2) {
    return 1;  // Exhausted path, not found
  } else if (tok == '}' || tok == ']') {
    data->d1--;
    // data->d2--;
    if (!data->path[data->pos] && data->d1 == data->d2 && data->obj != -1) {
      data->tok = tok - 2;
      if (data->tokptr) *data->tokptr = s + data->obj;
      if (data->toklen) *data->toklen = off - data->obj + 1;
      return 1;
    }
  } else if (MJSON_TOK_IS_VALUE(tok)) {
    // printf("TOK --> %d\n", tok);
    if (data->d1 == data->d2 && !data->path[data->pos]) {
      data->tok = tok;
      if (data->tokptr) *data->tokptr = s + off;
      if (data->toklen) *data->toklen = len;
      return 1;
    }
  }
  return 0;
}

enum mjson_tok ATTR mjson_find(const char *s, int len, const char *jp,
                               const char **tokptr, int *toklen) {
  struct msjon_get_data data = {jp, 1,  0,      0,      0,
                                0,  -1, tokptr, toklen, MJSON_TOK_INVALID};
  if (jp[0] != '$') return MJSON_TOK_INVALID;
  if (mjson(s, len, mjson_get_cb, &data) < 0) return MJSON_TOK_INVALID;
  return (enum mjson_tok) data.tok;
}

int mjson_get_number(const char *s, int len, const char *path, double *v) {
  const char *p;
  int tok, n;
  if ((tok = mjson_find(s, len, path, &p, &n)) == MJSON_TOK_NUMBER) {
    if (v != NULL) *v = strtod(p, NULL);
  }
  return tok == MJSON_TOK_NUMBER ? 1 : 0;
}

int ATTR mjson_get_bool(const char *s, int len, const char *path, int *v) {
  int tok = mjson_find(s, len, path, NULL, NULL);
  if (tok == MJSON_TOK_TRUE && v != NULL) *v = 1;
  if (tok == MJSON_TOK_FALSE && v != NULL) *v = 0;
  return tok == MJSON_TOK_TRUE || tok == MJSON_TOK_FALSE ? 1 : 0;
}

static int ATTR mjson_unescape(const char *s, int len, char *to, int n) {
  int i, j;
  for (i = 0, j = 0; i < len && j < n; i++, j++) {
    if (s[i] == '\\' && i + 1 < len) {
      int c = mjson_esc(s[i + 1], 0);
      if (c == 0) return -1;
      to[j] = c;
      i++;
    } else {
      to[j] = s[i];
    }
  }
  if (j >= n) return -1;
  if (n > 0) to[j] = '\0';
  return j;
}

int ATTR mjson_get_string(const char *s, int len, const char *path, char *to,
                          int n) {
  const char *p;
  int sz;
  if (mjson_find(s, len, path, &p, &sz) != MJSON_TOK_STRING) return -1;
  return mjson_unescape(p + 1, sz - 2, to, n);
}

int ATTR mjson_get_hex(const char *s, int len, const char *x, char *to, int n) {
  const char *p;
  int i, j, sz;
  if (mjson_find(s, len, x, &p, &sz) != MJSON_TOK_STRING) return -1;
  for (i = j = 0; i < sz - 3 && j < n; i += 2, j++) {
#define HEXTOI(x) (x >= '0' && x <= '9' ? x - '0' : x - 'W')
    unsigned char a = *(const unsigned char *) (p + i + 1);
    unsigned char b = *(const unsigned char *) (p + i + 2);
    ((unsigned char *) to)[j] = (HEXTOI(a) << 4) | HEXTOI(b);
  }
  if (j < n) to[j] = '\0';
  return j;
}

struct nextdata {
  int off, len, depth, t, vo, arrayindex;
  int *koff, *klen, *voff, *vlen, *vtype;
};

static int ATTR next_cb(int tok, const char *s, int off, int len, void *ud) {
  struct nextdata *d = (struct nextdata *) ud;
  // int i;
  switch (tok) {
    case '{':
    case '[':
      if (d->depth == 0 && tok == '[') d->arrayindex = 0;
      if (d->depth == 1 && off > d->off) {
        d->vo = off;
        d->t = tok == '{' ? MJSON_TOK_OBJECT : MJSON_TOK_ARRAY;
        if (d->voff) *d->voff = off;
        if (d->vtype) *d->vtype = d->t;
      }
      d->depth++;
      break;
    case '}':
    case ']':
      d->depth--;
      if (d->depth == 1 && d->vo) {
        d->len = off + len;
        if (d->vlen) *d->vlen = d->len - d->vo;
        if (d->arrayindex >= 0) {
          if (d->koff) *d->koff = d->arrayindex;  // koff holds array index
          if (d->klen) *d->klen = 0;              // klen holds 0
        }
        return 1;
      }
      if (d->depth == 1 && d->arrayindex >= 0) d->arrayindex++;
      break;
    case ',':
    case ':':
      break;
    case MJSON_TOK_KEY:
      if (d->depth == 1 && d->off < off) {
        if (d->koff) *d->koff = off;  // And report back to the user
        if (d->klen) *d->klen = len;  // If we have to
      }
      break;
    default:
      if (d->depth != 1) break;
      // If we're iterating over the array
      if (off > d->off) {
        d->len = off + len;
        if (d->vlen) *d->vlen = len;    // value length
        if (d->voff) *d->voff = off;    // value offset
        if (d->vtype) *d->vtype = tok;  // value type
        if (d->arrayindex >= 0) {
          if (d->koff) *d->koff = d->arrayindex;  // koff holds array index
          if (d->klen) *d->klen = 0;              // klen holds 0
        }
        return 1;
      }
      if (d->arrayindex >= 0) d->arrayindex++;
      break;
  }
  (void) s;
  return 0;
}

int ATTR mjson_next(const char *s, int n, int off, int *koff, int *klen,
                    int *voff, int *vlen, int *vtype) {
  struct nextdata d = {off, 0, 0, 0, 0, -1, koff, klen, voff, vlen, vtype};
  mjson(s, n, next_cb, &d);
  return d.len;
}
