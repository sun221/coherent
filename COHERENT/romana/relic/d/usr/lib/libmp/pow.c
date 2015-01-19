#include "mprec.h"


/*
 *	Pow sets the mint pointed to by "c" to the mint pointed to by "a"
 *	raised to the mint pointed to by "b" power reduced modulo the
 *	mint pointed to by "m".  If "b" is negative then
 *	mperr is called with the appropriate error message.
 *	Note that no assumption is made as to the distinctness of "a", "b",
 *	"m" and "c".
 */

void
pow(a, b, m, c)
mint *a, *b, *c;
register mint *m;
{
	mint	al, bl, cl, quot;
	int rem;

	if (!ispos(b))
		mperr("negative power");

	/* make local copies of a (reduced mod m) and b */
	minit(&al);
	minit(&quot);
	mdiv(a, m, &quot, &al);
	minit(&bl);
	mcopy(b, &bl);

	/* form actual power */
	minit(&cl);
	sdiv(&bl, 2, &bl, &rem);
	if (rem != 0)
		mcopy(&al, &cl);
	else
		mcopy(mone, &cl);
	while (!zerop(&bl)) {
		mult(&al, &al, &al);
		mdiv(&al, m, &quot, &al);
		sdiv(&bl, 2, &bl, &rem);
		if (rem != 0) {
			mult(&cl, &al, &cl);
			mdiv(&cl, m, &quot, &cl);
		}
	}

	/* clean up garbage */
	mpfree(al.val);
	mpfree(bl.val);
	mintfr(quot.val);
	*c = cl;
}
