/*
* ae.h
* evaluate arithmetic expressions at run time
* Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
* 06 May 2010 23:45:53
* This code is hereby placed in the public domain.
*/

/*!
    Opens ae to be used. Call it once before calling the others.
    Does nothing if ae is already open.
*/
void ae_open(void);

/*!
    Closes ae after use. All variables are deleted.
    Does nothing if ae is already closed.
*/
void ae_close(void);

/*!
    Sets the value of a variable.
    The value persists until it is set again or ae is closed.
*/
double ae_set(const char *name, double value);

/*!
    Evaluates the given expression and returns its value.
    Once ae has seen an expression, ae can evaluate it repeatedly quickly.
    Returns 0 if there is an error.  ae_error returns the error message.
*/
double ae_eval(const char *expression);

/*!
    Returns the last error message or NULL if there is none.
*/
const char *ae_error(void);

/*- End of file ------------------------------------------------------------*/
