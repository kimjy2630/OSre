#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;
printf("%d\n", argc);
  for (i = 0; i < argc; i++)
    printf ("%d %s ",i, argv[i]);
  printf ("\n");

  return EXIT_SUCCESS;
}
