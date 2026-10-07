#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int argc, pos;
static char **argv;

static void fail(const char *message) {
  fprintf(stderr, "bootstrap expr: %s\n", message);
  exit(2);
}

static char *number(long long value) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%lld", value);
  char *result = strdup(buffer);
  if (!result) fail("out of memory");
  return result;
}

static int integer(const char *s, long long *value) {
  char *end;
  if (!*s || (*s == '-' && !s[1])) return 0;
  errno = 0;
  *value = strtoll(s, &end, 10);
  return !*end && errno != ERANGE;
}

static int truth(char *s) {
  long long n;
  return *s && (!integer(s, &n) || n != 0);
}

static int precedence(char *s) {
  if (!strcmp(s, "|")) return 1;
  if (!strcmp(s, "&")) return 2;
  if (!strcmp(s, "=") || !strcmp(s, "!=") || !strcmp(s, "<") ||
      !strcmp(s, "<=") || !strcmp(s, ">") || !strcmp(s, ">=")) return 3;
  if (!strcmp(s, "+") || !strcmp(s, "-")) return 4;
  if (!strcmp(s, "*") || !strcmp(s, "/") || !strcmp(s, "%")) return 5;
  if (!strcmp(s, ":")) return 6;
  return 0;
}

static char *match(char *text, char *pattern) {
  regex_t regex;
  regmatch_t matches[2];
  char error[256];
  int result = regcomp(&regex, pattern, 0);
  if (result) {
    regerror(result, &regex, error, sizeof(error));
    fail(error);
  }
  result = regexec(&regex, text, 2, matches, 0);
  int capture = regex.re_nsub != 0;
  if (result == REG_NOMATCH || (!result && matches[0].rm_so != 0)) {
    regfree(&regex);
    return capture ? "" : "0";
  }
  if (result) {
    regerror(result, &regex, error, sizeof(error));
    regfree(&regex);
    fail(error);
  }
  if (!capture) {
    long long length = matches[0].rm_eo;
    regfree(&regex);
    return number(length);
  }
  if (matches[1].rm_so < 0) {
    regfree(&regex);
    return "";
  }
  size_t length = matches[1].rm_eo - matches[1].rm_so;
  char *value = malloc(length + 1);
  if (!value) fail("out of memory");
  memcpy(value, text + matches[1].rm_so, length);
  value[length] = 0;
  regfree(&regex);
  return value;
}

static char *apply(char *a, char *op, char *b) {
  if (!strcmp(op, ":")) return match(a, b);
  if (!strcmp(op, "|")) return truth(a) ? a : truth(b) ? b : "0";
  if (!strcmp(op, "&")) return truth(a) && truth(b) ? a : "0";
  long long x, y;
  int numeric = integer(a, &x) && integer(b, &y);
  if (precedence(op) == 3) {
    int c = numeric ? (x > y) - (x < y) : strcmp(a, b);
    if (!strcmp(op, "=")) return number(c == 0);
    if (!strcmp(op, "!=")) return number(c != 0);
    if (!strcmp(op, "<")) return number(c < 0);
    if (!strcmp(op, "<=")) return number(c <= 0);
    if (!strcmp(op, ">")) return number(c > 0);
    return number(c >= 0);
  }
  if (!numeric) fail("integer argument required");
  if (!strcmp(op, "+")) {
    if ((y > 0 && x > LLONG_MAX - y) || (y < 0 && x < LLONG_MIN - y))
      fail("integer overflow");
    return number(x + y);
  }
  if (!strcmp(op, "-")) {
    if ((y < 0 && x > LLONG_MAX + y) || (y > 0 && x < LLONG_MIN + y))
      fail("integer overflow");
    return number(x - y);
  }
  if (!strcmp(op, "*")) {
    if (x && y && ((x == -1 && y == LLONG_MIN) ||
        (y == -1 && x == LLONG_MIN) ||
        (x > 0 ? (y > 0 ? x > LLONG_MAX / y : y < LLONG_MIN / x) :
                 (y > 0 ? x < LLONG_MIN / y : x < LLONG_MAX / y))))
      fail("integer overflow");
    return number(x * y);
  }
  if (!y) fail("division by zero");
  if (x == LLONG_MIN && y == -1) fail("integer overflow");
  return number(!strcmp(op, "/") ? x / y : x % y);
}

static char *expression(int minimum) {
  if (pos >= argc) fail("missing operand");
  char *value;
  if (!strcmp(argv[pos], "(")) {
    pos++;
    value = expression(1);
    if (pos >= argc || strcmp(argv[pos++], ")")) fail("missing ')'");
  } else {
    value = argv[pos++];
  }
  while (pos < argc && precedence(argv[pos]) >= minimum) {
    char *op = argv[pos++];
    char *right = expression(precedence(op) + 1);
    value = apply(value, op, right);
  }
  return value;
}

int main(int count, char **args) {
  argc = count;
  argv = args;
  pos = count > 1 && !strcmp(args[1], "--") ? 2 : 1;
  char *value = expression(1);
  if (pos != argc) fail("unexpected argument");
  puts(value);
  return truth(value) ? 0 : 1;
}
