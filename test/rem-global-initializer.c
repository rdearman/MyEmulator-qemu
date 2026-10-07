/* Regression for incoming stack-argument offsets in write_gvar_data.
 * The fifth parameter is stack-passed; a global initializer exercises the
 * same five-argument recursive call path during hosted compilation. */
int value = 1;

int main(void) {
  return value != 1;
}
