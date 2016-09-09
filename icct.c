#include <math.h>

/* Credit: https://en.wikipedia.org/wiki/Planckian_locus#Approximation
 * As stated there, only accurate in ~[1667K, 25kK].
 * Domain is real (contingent above); range is [0, 1] on both components.
 */
void kelvin2ciexy(double K, double *x, double *y) {
	if(K < 4000) {
		*x =  -0.2661239e9 / (K * K * K) \
			 - 0.2343580e6 / (K * K) \
			 + 0.8776956e3 / K \
			 + 0.179910;
	} else {
		*x =  -3.0258469e9 / (K * K * K) \
			 + 2.1070379e6 / (K * K) \
			 + 0.2226347e3 / K \
			 + 0.240390;
	}
	if(K < 2222) {
		*y =  -1.1063814 * (*x) * (*x) * (*x) \
			 - 1.34811020 * (*x) * (*x) \
			 + 2.18555832 * (*x) \
			 - 0.20219683;
	} else if(K < 4000) {
		*y =  -0.9549476 * (*x) * (*x) * (*x) \
			 - 1.37418593 * (*x) * (*x) \
			 + 2.09137015 * (*x) \
			 - 0.16748867;
	} else {
		*y =   3.0817580 * (*x) * (*x) * (*x) \
			 - 5.87338670 * (*x) * (*x) \
			 + 3.75112997 * (*x) \
			 - 0.37001483;
	}
}

/* Credit: https://en.wikipedia.org/wiki/SRGB#Specification_of_the_transformation
 * Domain and range are all [0, 1].
 * Output is *linear*, scale later (if needed) with srgbgamma().
 */
void ciexyy2srgb(double x, double y, double Y, double *r, double *g, double *b) {
	double X = Y * x / y, Z = Y * (1 - x - y) / y;
	*r =  3.2406 * X - 1.5372 * Y - 0.4986 * Z;
	*g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
	*b =  0.0557 * X - 0.2040 * Y + 1.0570 * Z;
}

/* See above; domain and range are [0, 1]--this operates in place. */
void srgbgamma(double *c) {
	if(*c <= 0.0031308) {
		*c *= 12.92;
	} else {
		*c = 1.055 * pow(*c, 1/2.4) - 0.055;
	}
}
