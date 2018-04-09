/* Make sure this is not misinterpreted as sizeof ((int)(*2)) */
int a = sizeof(int) * 2;

/* array[0] must be parsed as an expression, not a type name */
int b = sizeof(array) / sizeof(array[0]);

/* Expression */
int c = sizeof(a * b);

/* Type name */
int d = sizeof(foo_t *);