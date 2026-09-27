/* Regression for REM loop labels: a switch nested in an option-style
 * loop must break only its switch, while the surrounding loops retain
 * their own continue/back-edge targets. */
int main(int argc, char **argv) {
  int seen = 0;

  for (;;) {
    if (argc > 0)
      ++argv, --argc;
    if (argc == 0 || (*argv)[0] != '-')
      break;

    for (char *opt = &(*argv)[1], done = 0; !done && *opt; ++opt) {
      switch (*opt) {
      case 'n':
        ++seen;
        break;
      default:
        done = 1;
        break;
      }
    }
  }

  return seen == 1 ? 0 : 1;
}
