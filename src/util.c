/*
  Utility macros and functions

  See the accompanying LICENSE file.
*/

/* These macros are to address several issues:

  - Prevent simultaneous calls on the same object while the GIL is
  released in one thread.  For example if a Cursor is executing
  sqlite3_step with the GIL released, we don't want Cursor_execute
  called on another thread since that will thrash what the first
  thread is doing.  We use a member of Connection, Blob and Cursor
  named 'inuse' to provide the simple exclusion.

  - The GIL has to be released around all SQLite calls that take the
  database mutex (which is most of them).  If the GIL is kept even for
  trivial calls then deadlock will arise.  This is because if you have
  multiple mutexes you must always acquire them in the same order, or
  never hold more than one at a time.

  - The SQLite error code is not threadsafe.  This is because the
  error string is per database connection.  The call to sqlite3_errmsg
  will return a pointer but that can be replaced by any other thread
  with an error.  Consequently SQLite added sqlite3_db_mutex (see
  sqlite-dev mailing list for 4 Nov 2008).  A far better workaround
  would have been to make the SQLite error stuff be per thread just
  like errno.  Instead I have had to roll my own thread local storage
  system for storing the error message.
*/

/* call where no error is returned */
#define _PYSQLITE_CALL_V(x) \
  do                        \
  {                         \
    Py_BEGIN_ALLOW_THREADS  \
    {                       \
      x;                    \
    }                       \
    Py_END_ALLOW_THREADS;   \
  } while (0)

/* Calls where error could be set.  We assume that a variable 'res' is set.  Also need the db to take
   the mutex on */
#define _PYSQLITE_CALL_E(db, x)                                        \
  do                                                                   \
  {                                                                    \
    Py_BEGIN_ALLOW_THREADS                                             \
    {                                                                  \
      sqlite3_mutex_enter(sqlite3_db_mutex(db));                       \
      x;                                                               \
      if (res != SQLITE_OK && res != SQLITE_DONE && res != SQLITE_ROW) \
        apsw_set_errmsg(sqlite3_errmsg((db)));                         \
      sqlite3_mutex_leave(sqlite3_db_mutex(db));                       \
    }                                                                  \
    Py_END_ALLOW_THREADS;                                              \
  } while (0)

#define INUSE_CALL(x)         \
  do                          \
  {                           \
    assert(self->inuse == 0); \
    self->inuse = 1;          \
    {                         \
      x;                      \
    }                         \
    assert(self->inuse == 1); \
    self->inuse = 0;          \
  } while (0)

/* call from blob code */
#define PYSQLITE_BLOB_CALL(y) INUSE_CALL(_PYSQLITE_CALL_E(self->connection->db, y))

/* call from connection code */
#define PYSQLITE_CON_CALL(y) INUSE_CALL(_PYSQLITE_CALL_E(self->db, y))

/* call from cursor code - same as blob */
#define PYSQLITE_CUR_CALL PYSQLITE_BLOB_CALL

/* from statement cache */
#define PYSQLITE_SC_CALL(y) _PYSQLITE_CALL_E(sc->db, y)

/* call to sqlite code that doesn't return an error */
#define PYSQLITE_VOID_CALL(y) INUSE_CALL(_PYSQLITE_CALL_V(y))

/* call from backup code */
#define PYSQLITE_BACKUP_CALL(y) INUSE_CALL(_PYSQLITE_CALL_E(self->dest->db, y))

/*
   The default Python PyErr_WriteUnraisable is almost useless, and barely used
   by CPython.  It gives the developer no clue whatsoever where in
   the code it is happening.  It also does funky things to the passed
   in object which can cause the destructor to fire twice.
   Consequently we use our version here.  It makes the traceback
   complete, and then tries the following, going to the next if
   the hook isn't found or returns an error:

   * excepthook of hookobject (if not NULL)
   * unraisablehook of sys module
   * excepthook of sys module
   * PyErr_Display

   If any return an error then then the next one is tried.  When we
   return, any error will be cleared.
*/

/* used for calling sys.unraisablehook */
static PyStructSequence_Field apsw_unraisable_info_fields[] =
    {
        {"exc_type", "Exception type"},
        {"exc_value", "Execption value, can be None"},
        {"exc_traceback", "Exception traceback, can be None"},
        {"err_msg", "Error message, can be None"},
        {"object", "Object causing the exception, can be None"},
        {0}};

static PyStructSequence_Desc apsw_unraisable_info = {
    .name = "apsw.unraisable_info",
    .doc = "Glue for sys.unraisablehook",
    .n_in_sequence = 5,
    .fields = apsw_unraisable_info_fields};

static PyTypeObject apsw_unraisable_info_type;

static void
apsw_write_unraisable(PyObject *hookobject)
{
  assert(PyErr_Occurred());

  PyObject *err_type = NULL, *err_value = NULL, *err_traceback = NULL;
  PyObject *excepthook = NULL;
  PyObject *result = NULL;

  /* fill in the rest of the traceback */
#ifdef PYPY_VERSION
  /* do nothing */
#else
#if PY_VERSION_HEX < 0x03090000
  PyFrameObject *frame = PyThreadState_GET()->frame;
  while (frame)
  {
    PyTraceBack_Here(frame);
    frame = frame->f_back;
  }
#else
  PyFrameObject *prev = NULL, *frame = PyThreadState_GetFrame(PyThreadState_GET());
  while (frame)
  {
    PyTraceBack_Here(frame);
    prev = PyFrame_GetBack(frame);
    Py_DECREF(frame);
    frame = prev;
  }
#endif
#endif

  /* Get the exception details */
  PyErr_Fetch(&err_type, &err_value, &err_traceback);
  PyErr_NormalizeException(&err_type, &err_value, &err_traceback);

  int can_recurse = !Py_EnterRecursiveCall(" in apsw_write_unraisable");
  if (!can_recurse)
  {
    PyErr_Print();
    goto finally;
  }

  /* tell sqlite3_log */
  if (err_value)
  {
    PyObject *message = PyObject_Str(err_value);
    const char *utf8 = message ? PyUnicode_AsUTF8(message) : "failed to get string of error";
    PyErr_Clear();
    sqlite3_log(SQLITE_ERROR, "apsw_write_unraisable %s: %s", Py_TypeName(err_value), utf8);
    Py_CLEAR(message);
  }

  if (hookobject)
  {
    excepthook = PyObject_GetAttrString(hookobject, "excepthook");
    PyErr_Clear();
    if (excepthook)
    {
      result = PyObject_CallFunction(excepthook, "(OOO)", OBJ(err_type), OBJ(err_value), OBJ(err_traceback));
      if (result)
        goto finally;
    }
    Py_CLEAR(excepthook);
  }

  excepthook = PySys_GetObject("unraisablehook");
  if (excepthook)
  {
    Py_INCREF(excepthook); /* borrowed reference from PySys_GetObject so we increment */
    PyErr_Clear();
    PyObject *arg = PyStructSequence_New(&apsw_unraisable_info_type);
    if (arg)
    {
      PyStructSequence_SetItem(arg, 0, Py_NewRef(OBJ(err_type)));
      PyStructSequence_SetItem(arg, 1, Py_NewRef(OBJ(err_value)));
      PyStructSequence_SetItem(arg, 2, Py_NewRef(OBJ(err_traceback)));
      result = PyObject_CallFunction(excepthook, "(N)", arg);
      if (result)
        goto finally;
    }
    Py_CLEAR(excepthook);
  }

  excepthook = PySys_GetObject("excepthook");
  if (excepthook)
  {
    Py_INCREF(excepthook); /* borrowed reference from PySys_GetObject so we increment */
    PyErr_Clear();
    result = PyObject_CallFunction(excepthook, "(OOO)", OBJ(err_type), OBJ(err_value), OBJ(err_traceback));
    if (result)
      goto finally;
  }

  /* remove any error from callback failure since we'd have to call
     ourselves to raise it! */
  PyErr_Clear();
  PyErr_Display(err_type, err_value, err_traceback);

finally:
  Py_XDECREF(excepthook);
  Py_XDECREF(result);
  Py_XDECREF(err_traceback);
  Py_XDECREF(err_value);
  Py_XDECREF(err_type);
  PyErr_Clear(); /* being paranoid - make sure no errors on return */
  if (can_recurse)
    Py_LeaveRecursiveCall();
}

#undef convert_value_to_pyobject
/* Converts sqlite3_value to PyObject.  Returns a new reference. */
static PyObject *
convert_value_to_pyobject(sqlite3_value *value, int in_constraint_possible, int no_change_possible)
{
#include "faultinject.h"

  int coltype = sqlite3_value_type(value);
  sqlite3_value *in_value;

  if (no_change_possible && sqlite3_value_nochange(value))
    return Py_NewRef(&apsw_no_change_object);

  switch (coltype)
  {
  case SQLITE_INTEGER:
  {
    sqlite3_int64 val = sqlite3_value_int64(value);
    return PyLong_FromLongLong(val);
  }

  case SQLITE_FLOAT:
    return PyFloat_FromDouble(sqlite3_value_double(value));

  case SQLITE_TEXT:
    assert(sqlite3_value_text(value));
    return PyUnicode_FromStringAndSize((const char *)sqlite3_value_text(value), sqlite3_value_bytes(value));

  default:
  case SQLITE_NULL:
    if (in_constraint_possible && sqlite3_vtab_in_first(value, &in_value) == SQLITE_OK)
    {
      int res;
      PyObject *v = NULL, *set = PySet_New(NULL);
      if (!set)
        return NULL;
      while (in_value)
      {
        v = convert_value_to_pyobject(in_value, 0, 0);
        if (!v || 0 != PySet_Add(set, v))
          goto error;
        Py_CLEAR(v);
        res = sqlite3_vtab_in_next(value, &in_value);
        if (res != SQLITE_DONE && res != SQLITE_OK)
        {
          /* this should use SET_EXC but there is a circular dependency between that
             file and this one, so we punt on this unlikely scenario */
          PyErr_Format(PyExc_ValueError, "Failed in sqlite3_vtab_in_next result %d", res);
          goto error;
        }
      }
      return set;
    error:
      Py_XDECREF(v);
      Py_XDECREF(set);
      return NULL;
    }
    Py_RETURN_NONE;

  case SQLITE_BLOB:
    return PyBytes_FromStringAndSize(sqlite3_value_blob(value), sqlite3_value_bytes(value));
  }
}

static PyObject *
convert_value_to_pyobject_not_in(sqlite3_value *value)
{
  return convert_value_to_pyobject(value, 0, 0);
}

/* Converts column to PyObject.  Returns a new reference. Almost identical to above
   but we cannot just use sqlite3_column_value and then call the above function as
   SQLite doesn't allow that ("unprotected values") */
static PyObject *
convert_column_to_pyobject(sqlite3_stmt *stmt, int col)
{
  int coltype;

  _PYSQLITE_CALL_V(coltype = sqlite3_column_type(stmt, col));

  switch (coltype)
  {
  case SQLITE_INTEGER:
  {
    sqlite3_int64 val;
    _PYSQLITE_CALL_V(val = sqlite3_column_int64(stmt, col));
    return PyLong_FromLongLong(val);
  }

  case SQLITE_FLOAT:
  {
    double d;
    _PYSQLITE_CALL_V(d = sqlite3_column_double(stmt, col));
    return PyFloat_FromDouble(d);
  }
  case SQLITE_TEXT:
  {
    const char *data;
    size_t len;
    _PYSQLITE_CALL_V((data = (const char *)sqlite3_column_text(stmt, col), len = sqlite3_column_bytes(stmt, col)));
    return PyUnicode_FromStringAndSize(data, len);
  }

  default:
  case SQLITE_NULL:
    Py_RETURN_NONE;

  case SQLITE_BLOB:
  {
    const void *data;
    size_t len;
    _PYSQLITE_CALL_V((data = sqlite3_column_blob(stmt, col), len = sqlite3_column_bytes(stmt, col)));
    return PyBytes_FromStringAndSize(data, len);
  }
  }
}

/* Some macros used for frequent operations */

/* used by Connection and Cursor */
#define CHECK_USE(e)                                                                                                                                                           \
  do                                                                                                                                                                           \
  {                                                                                                                                                                            \
    if (self->inuse)                                                                                                                                                           \
    { /* raise exception if we aren't already in one */                                                                                                                        \
      if (!PyErr_Occurred())                                                                                                                                                   \
        PyErr_Format(ExcThreadingViolation, "You are trying to use the same object concurrently in two threads or re-entrantly within the same thread which is not allowed."); \
      return e;                                                                                                                                                                \
    }                                                                                                                                                                          \
  } while (0)

/* used by Connection */
#define CHECK_CLOSED(connection, e)                                        \
  do                                                                       \
  {                                                                        \
    if (!(connection) || !(connection)->db)                                \
    {                                                                      \
      PyErr_Format(ExcConnectionClosed, "The connection has been closed"); \
      return e;                                                            \
    }                                                                      \
  } while (0)

/* used by cursor */
#define CHECK_CURSOR_CLOSED(e)                                             \
  do                                                                       \
  {                                                                        \
    if (!self->connection)                                                 \
    {                                                                      \
      PyErr_Format(ExcCursorClosed, "The cursor has been closed");         \
      return e;                                                            \
    }                                                                      \
    else if (!self->connection->db)                                        \
    {                                                                      \
      PyErr_Format(ExcConnectionClosed, "The connection has been closed"); \
      return e;                                                            \
    }                                                                      \
  } while (0)

#undef apsw_strdup
/* This adds double nulls on the end - needed if string is a filename
   used near vfs as SQLite puts extra info after the first null */
static char *apsw_strdup(const char *source)
{
#include "faultinject.h"

  size_t len = strlen(source);
  char *res = PyMem_Calloc(1, len + 3);
  if (res)
  {
    res[len] = res[len + 1] = res[len + 2] = 0;
    PyOS_snprintf(res, len + 1, "%s", source);
  }
  return res;
}

/*

Some SQLite methods can only be called from within callbacks (eg
sqlite3_vtab_config can only be called when in xCreate/xConnect).
These macros help implement that.

For example, to use for Connection and xCreate:

- Add CALL_TRACK(xCreate) in struct Connection

- Add CALL_ENTER(xCreate) at the start of the xCreate callback

- Add CALL_EXIT(xCreate) at the end of the xCreate callback

- Use CALL_CHECK(xCreate) where sqlite3_vtab_config could be called
  to see if we are in an xCreate call

The implementation uses a sentinel on the stack with a magic number on
it.  That way if the call exits without CALL_LEAVE a later check will
assert fail or cause a valgrind error.

*/

#define CALL_TRACK(name) \
  int *in_call##name

#define CALL_TRACK_INIT(name) self->in_call##name = NULL

#define CALL_ENTER(name)                                                         \
  assert(self->in_call##name == NULL || *(self->in_call##name) == MAGIC_##name); \
  int *enter_call_save_##name = self->in_call##name;                             \
  int enter_call_here_##name = MAGIC_##name;                                     \
  self->in_call##name = &enter_call_here_##name

#define CALL_LEAVE(name) \
  self->in_call##name = enter_call_save_##name

#define CALL_CHECK(name) (self->in_call##name != NULL)

/* some arbitrary magic numbers for call track */
#define MAGIC_xConnect 0x008295ab
#define MAGIC_xUpdate 0x119306bc