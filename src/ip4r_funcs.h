/* ip4r_funcs.h */

static inline
uint32 hostmask(unsigned masklen)
{
    return (masklen) ? ( (((uint32)(1U)) << (32-masklen)) - 1U ) : 0xFFFFFFFFU;
}

static inline
uint32 netmask(unsigned masklen)
{
    return ~hostmask(masklen);
}

/* if LO and HI are ends of a CIDR prefix, return the mask length.
 * If not, returns ~0.
 */

static inline
unsigned masklen(uint32 lo, uint32 hi)
{
    uint32 d = (lo ^ hi) + 1;
    /* at this point, d can be:
     *  0 if A and B have all bits different
     *  1 if A and B are equal
     *  1 << (32-masklen)
     *  some other value if A and B are not ends of a CIDR range
     * but in any case, after extracting the masklen, we have to
     * recheck because some non-CIDR ranges will produce the same
     * results.
     */
    int fbit = ffs(d);
    switch (fbit)
    {
        case 0: return (lo == 0 && hi == ~0) ? 0 : ~0;
        case 1: return (lo == hi) ? 32 : ~0;
        default:
            if ( ((uint32)(1U) << (fbit-1)) == d )
            {
                uint32 mask = hostmask(33-fbit);
                if ((lo & mask) == 0 && (hi & mask) == mask)
                    return 33-fbit;
            }
            return ~0;
    }
}

static inline
bool ip4_valid_netmask(uint32 mask)
{
    uint32 d = ~mask + 1;
    /* at this point, d can be:
     *  0 if mask was 0x00000000 (valid)
     *  1 << (32-masklen)   (valid)
     *  some other value  (invalid)
     */
    int fbit = ffs(d);
    switch (fbit)
    {
        case 0:
            return true;
        default:
            return ( ((uint32)(1U) << (fbit-1)) == d );
    }
}


static inline
bool ip4r_from_cidr(IP4 prefix, unsigned masklen, IP4R *ipr)
{
    uint32 mask = hostmask(masklen);
    if (masklen > 32)
        return false;
    if (prefix & mask)
        return false;
    ipr->lower = prefix;
    ipr->upper = prefix | mask;
    return true;
}

static inline
bool ip4r_from_inet(IP4 addr, unsigned masklen, IP4R *ipr)
{
    uint32 mask = hostmask(masklen);
    if (masklen > 32)
        return false;
    ipr->lower = addr & ~mask;
    ipr->upper = addr | mask;
    return true;
}

static inline
bool ip4r_split_cidr(IP4R *val, IP4R *res)
{
	IP4 lo = val->lower;
	IP4 hi = val->upper;
	int len = 32;
	IP4 mask = 1;

	res->lower = lo;
	res->upper = hi;

	if (masklen(lo,hi) <= 32U)
		return true;

	/*
	 * We know that all these cases are impossible because the CIDR check
	 * would catch them:
	 *
	 *  lo==hi
	 *  lo==0 && hi==~0
	 *  lo==~0
	 *
	 * Therefore this loop must terminate before the mask overflows
	 */
	while ((lo & mask) == 0 && (lo | mask) <= hi)
		--len, mask = (mask << 1) | 1;
	mask >>= 1;

	res->upper = lo | mask;
	val->lower = (lo | mask) + 1;

	return false;
}

/* comparisons */

static inline
bool ip4_equal(IP4 a, IP4 b)
{
    return (a == b);
}

static inline
bool ip4_lessthan(IP4 a, IP4 b)
{
    return (a < b);
}

static inline
bool ip4_less_eq(IP4 a, IP4 b)
{
    return (a <= b);
}

/* helpers for union/intersection for indexing */

static inline
IP4R *ip4r_union_internal(IP4R *a, IP4R *b, IP4R *result)
{
    if (a->lower < b->lower)
        result->lower = a->lower;
    else
        result->lower = b->lower;

    if (a->upper > b->upper)
        result->upper = a->upper;
    else
        result->upper = b->upper;

    return result;
}

static inline
IP4R *ip4r_inter_internal(IP4R *a, IP4R *b, IP4R *result)
{
    if (a->upper < b->lower || a->lower > b->upper)
    {
        /* disjoint */
        result->lower = 1;
        result->upper = 0;  /* INVALID VALUE */
        return NULL;
    }

    if (a->upper < b->upper)
        result->upper = a->upper;
    else
        result->upper = b->upper;

    if (a->lower > b->lower)
        result->lower = a->lower;
    else
        result->lower = b->lower;

    return result;
}

static inline
double ip4r_metric(IP4R *v)
{
    if (!v)
        return 0.0;
    return ((v->upper - v->lower) + 1.0);
}

static inline
bool ip4r_equal(IP4R *a, IP4R *b)
{
    return (a->lower == b->lower && a->upper == b->upper);
}

static inline
bool ip4r_lessthan(IP4R *a, IP4R *b)
{
    return (a->lower == b->lower) ? (a->upper < b->upper) : (a->lower < b->lower);
}

static inline
bool ip4r_less_eq(IP4R *a, IP4R *b)
{
    return (a->lower == b->lower) ? (a->upper <= b->upper) : (a->lower < b->lower);
}

static inline
bool ip4r_contains_internal(IP4R *left, IP4R *right, bool eqval)
{
    if (ip4r_equal(left,right))
        return eqval;
    return ((left->lower <= right->lower) && (left->upper >= right->upper));
}

static inline
bool ip4r_overlaps_internal(IP4R *left, IP4R *right)
{
    return (left->upper >= right->lower && left->lower <= right->upper);
}

static inline
bool ip4_contains_internal(IP4R *left, IP4 right)
{
    return (left->lower <= right && left->upper >= right);
}

static inline
bool ip4r_left_internal(IP4R *left, IP4R *right)
{
    return (left->upper < right->lower);
}

static inline
bool ip4r_extends_left_of_internal(IP4R *left, IP4R *right)
{
    return (left->lower < right->lower);
}

static inline
bool ip4r_extends_right_of_internal(IP4R *left, IP4R *right)
{
    return (left->upper > right->upper);
}

/* end */
