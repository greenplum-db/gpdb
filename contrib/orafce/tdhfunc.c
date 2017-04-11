/* 
 * Hash three letter week days to some number. The TDH hash table makes this
 * into a perfect hash for us. See generate_hash.c.
 */
static int
tdhfunc(const char *s)
{
	int i = 0;
	int ret = 0;

	while (i < 3)
	{
		int out = 0;

		switch (s[i])
		{
			case 'S':
			case 's':
				out += (int)'s';
				break;
			case 'U':
			case 'u':
				out += (int)'u';
				break;
			case 'N':
			case 'n':
				out += (int)'n';
				break;
			case 'M':
			case 'm':
				out += (int)'m';
				break;
			case 'O':
			case 'o':
				out += (int)'o';
				break;
			case 'T':
			case 't':
				out += (int)'t';
				break;
			case 'E':
			case 'e':
				out += (int)'e';
				break;
			case 'W':
			case 'w':
				out += (int)'w';
				break;
			case 'D':
			case 'd':
				out += (int)'d';
				break;
			case 'H':
			case 'h':
				out += (int)'h';
				break;
			case 'F':
			case 'f':
				out += (int)'f';
				break;
			case 'R':
			case 'r':
				out += (int)'r';
				break;
			case 'I':
			case 'i':
				out += (int)'i';
				break;
			case 'A':
			case 'a':
				out += (int)'a';
				break;
			default:
				return -1;
				break;
		}
		ret += out;
		i++;
	}
	return ret;
}
