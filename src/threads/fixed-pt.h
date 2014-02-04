/*! The file defines all the fixed-point arithmetic functions
 *  Note that for the fixed-point, we are using the 17.14
 *  format.*/

/*! N=2^14 is the converter for fixed-point.*/
#define FLT 16384

/*! Convert input n to a fixed-point*/
#define I2F(n) (n * FLT)
)
/*! Convert input fixed-put to an integer, round towards 0.*/
#define F2IZ(x) (x / FLT)

/*! Convert input fixed-put to an integer, round to nearest.*/
#define F2IN(x) ((x <= 0) ? ((x - FLT / 2) / FLT) : (( x + FLT / 2) / FLT))

/*! Add a float and an int, note fixed-pt is the first argument, and int is
 *  the second */
#define FADDI(x, n) (x + n * FLT)

/*! Subtract an int from a fixed-pt, note fixed-pt is the first argument, and
 *  the int is the second. */
#define FSUBI(x, n) (x - n * FLT)

/*! Multiply a fixed-pt with another fixed-pt*/
#define FMULF(x, y) (((int64_t) x) * y / FLT)

/*! Multiply a fixed-pt with an int, note fixed-pt is the first argument, and
 *  the int is the second.*/
#define FMULI(x, n) (x * n)

/*! Divide a fixed-pt by a fixed-pt.*/
#define FDIVF(x, y) (((int64_t) x) * FLT / y)

/*! Divide a fixed-pt by an int.*/
#define FDIVI(x, n) (x / n)
