/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                              |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2016-2020 M/Gateway Developments Ltd,                      |
   | Surrey UK.                                                               |
   | All rights reserved.                                                     |
   |                                                                          |
   | http://www.mgateway.com                                                  |
   |                                                                          |
   | Licensed under the Apache License, Version 2.0 (the "License"); you may  |
   | not use this file except in compliance with the License.                 |
   | You may obtain a copy of the License at                                  |
   |                                                                          |
   | http://www.apache.org/licenses/LICENSE-2.0                               |
   |                                                                          |
   | Unless required by applicable law or agreed to in writing, software      |
   | distributed under the License is distributed on an "AS IS" BASIS,        |
   | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
   | See the License for the specific language governing permissions and      |
   | limitations under the License.                                           |      
   |                                                                          |
   ----------------------------------------------------------------------------
*/

#include "mg-dbx.h"
#include "mg-cursor.h"
 
using namespace v8;
using namespace node;

Persistent<Function> mcursor::constructor;

mcursor::mcursor(int value) : dbx_count(value)
{
}


mcursor::~mcursor()
{
   delete_mcursor_template(this);
}


#if DBX_NODE_VERSION >= 100000
void mcursor::Init(Local<Object> exports)
#else
void mcursor::Init(Handle<Object> exports)
#endif
{
#if DBX_NODE_VERSION >= 120000
   Isolate* isolate = exports->GetIsolate();
   Local<Context> icontext = isolate->GetCurrentContext();

   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mcursor", NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#else
   Isolate* isolate = Isolate::GetCurrent();

   /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mcursor"));
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#endif

   /* Prototypes */

   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "execute", Execute);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "cleanup", Cleanup);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "next", Next);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "previous", Previous);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "reset", Reset);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "_close", Close);

#if DBX_NODE_VERSION >= 120000
   constructor.Reset(isolate, tpl->GetFunction(icontext).ToLocalChecked());
   exports->Set(icontext, String::NewFromUtf8(isolate, "mcursor", NewStringType::kNormal).ToLocalChecked(), tpl->GetFunction(icontext).ToLocalChecked()).FromJust();
#else
   constructor.Reset(isolate, tpl->GetFunction());
#endif

}


void mcursor::New(const FunctionCallbackInfo<Value>& args)
{
   Isolate* isolate = args.GetIsolate();
#if DBX_NODE_VERSION >= 100000
   Local<Context> icontext = isolate->GetCurrentContext();
#endif
   HandleScope scope(isolate);
   int rc, fc, mn, argc, otype;
   DBX_DBNAME *c = NULL;
   Local<Object> obj;

   /* 1.4.10 */
   argc = args.Length();
   if (argc > 0) {
      obj = dbx_is_object(args[0], &otype);
      if (otype) {
         fc = obj->InternalFieldCount();
         if (fc == 3) {
            mn = DBX_INT32_VALUE(obj->GetInternalField(2));
            if (mn == DBX_MAGIC_NUMBER) {
               c = ObjectWrap::Unwrap<DBX_DBNAME>(obj);
            }
         }
      }
   }

   if (args.IsConstructCall()) {
      /* Invoked as constructor: `new mcursor(...)` */
      int value = args[0]->IsUndefined() ? 0 : DBX_INT32_VALUE(args[0]);

      mcursor * obj = new mcursor(value);
      obj->c = NULL;

      if (c) { /* 1.4.10 */
         if (c->pcon == NULL) {
            isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "No Connection to the database", 1)));
            return;
         }
         c->pcon->argc = argc;
         dbx_cursor_init((void *) obj);
         obj->c = c;

         /* 1.4.10 */
         rc = dbx_cursor_reset(args, isolate, (void *) obj, 1, 1);
         if (rc < 0) {
            isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mcursor::New() method takes at least one argument (the query object)", 1)));
            return;
         }
      }
      obj->Wrap(args.This());
      args.GetReturnValue().Set(args.This());
   }
   else {
      /* Invoked as plain function `mcursor(...)`, turn into construct call. */
      const int argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
   }
}


mcursor * mcursor::NewInstance(const FunctionCallbackInfo<Value>& args)
{
   Isolate* isolate = args.GetIsolate();
   Local<Context> icontext = isolate->GetCurrentContext();
   HandleScope scope(isolate);

#if DBX_NODE_VERSION >= 100000
   Local<Value> argv[2];
#else
   Handle<Value> argv[2];
#endif
   const unsigned argc = 1;

   argv[0] = args[0];

   Local<Function> cons = Local<Function>::New(isolate, constructor);
   Local<Object> instance = cons->NewInstance(icontext, argc, argv).ToLocalChecked();
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(instance);

   args.GetReturnValue().Set(instance);

   return cx;
}


int mcursor::delete_mcursor_template(mcursor *cx)
{
   return 0;
}


void mcursor::Execute(const FunctionCallbackInfo<Value>& args)
{
   short async;
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> key;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->dbx_count ++;

   pcon = c->pcon;
   pcon->psql = cx->psql;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Execute", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_sql_execute;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);

      baton->cb.Reset(isolate, cb);

      cx->Ref();

      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback_sql_execute, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   dbx_sql_execute(pcon);

   DBX_DBFUN_END(c);

   obj = DBX_OBJECT_NEW();

   key = dbx_new_string8(isolate, (char *) "sqlcode", 0);
   DBX_SET(obj, key, DBX_INTEGER_NEW(pcon->psql->sqlcode));
   key = dbx_new_string8(isolate, (char *) "sqlstate", 0);
   DBX_SET(obj, key, dbx_new_string8(isolate, pcon->psql->sqlstate, 0));
   if (pcon->error[0]) {
      key = dbx_new_string8(isolate, (char *) "error", 0);
      DBX_SET(obj, key, dbx_new_string8(isolate, pcon->error, 0));
   }

   args.GetReturnValue().Set(obj);
}


void mcursor::Cleanup(const FunctionCallbackInfo<Value>& args)
{
   short async;
   DBXCON *pcon;
   Local<String> result;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ISOLATE;
   cx->dbx_count ++;

   pcon = c->pcon;
   pcon->psql = cx->psql;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Cleanup", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_sql_cleanup;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);

      baton->cb.Reset(isolate, cb);

      cx->Ref();

      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   dbx_sql_cleanup(pcon);

   DBX_DBFUN_END(c);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
   args.GetReturnValue().Set(result);
   return;
}


void mcursor::Next(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int n, eod;
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> key;
   DBXQR *pqr;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->dbx_count ++;

   pcon = c->pcon;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Next", 1)));
      return;
   }
   if (async) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Cursor based operations cannot be invoked asynchronously", 1)));
      return;
   }

   if (cx->context == 1) {
   
      if (cx->pqr_prev->keyn < 1) {
         args.GetReturnValue().Set(DBX_NULL());
         return;
      }

      DBX_DBFUN_START(c, pcon);
      DBX_DB_LOCK(n, 0);

      eod = dbx_global_order(pcon, cx->pqr_prev, 1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK(n);

      if (cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used == 0) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else if (cx->getdata == 0) {
/*
         if (cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used && cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used < 10) {
            int n;
            char buffer[32];
            strncpy(buffer, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used);
            buffer[cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used] = '\0';
            n = (int) strtol(buffer, NULL, 10);
            args.GetReturnValue().Set(DBX_INTEGER_NEW(n));
            return;
         }
*/
         key = dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, c->utf8);
         args.GetReturnValue().Set(key);
      }
      else {
         if (cx->format == 1) {
            cx->data.len_used = 0;
            dbx_escape_output(&(cx->data), (char *) "key=", 4, 0);
            dbx_escape_output(&(cx->data), cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, 1);
            dbx_escape_output(&(cx->data), (char *) "&data=", 6, 0);
            dbx_escape_output(&(cx->data), cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 1);
            key = dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
            args.GetReturnValue().Set(key);
         }
         else {
            obj = DBX_OBJECT_NEW();

            key = dbx_new_string8(isolate, (char *) "key", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, c->utf8));
            key = dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 0));
            args.GetReturnValue().Set(obj);
         }
      }
   }
   else if (cx->context == 2) {
   
      DBX_DBFUN_START(c, pcon);
      DBX_DB_LOCK(n, 0);

      eod = dbx_global_query(pcon, cx->pqr_next, cx->pqr_prev, 1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK(n);

      if (cx->format == 1) {
         char buffer[32], delim[4];

         cx->data.len_used = 0;
         *delim = '\0';
         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            sprintf(buffer, (char *) "%skey%d=", delim, n + 1);
            dbx_escape_output(&(cx->data), buffer, (int) strlen(buffer), 0);
            dbx_escape_output(&(cx->data), cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 1);
            strcpy(delim, (char *) "&");
         }
         if (cx->getdata) {
            sprintf(buffer, (char *) "%sdata=", delim);
            dbx_escape_output(&(cx->data), buffer, (int) strlen(buffer), 0);
            dbx_escape_output(&(cx->data), cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 1);
         }

         key = dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
         args.GetReturnValue().Set(key);
      }
      else {
         obj = DBX_OBJECT_NEW();
         key = dbx_new_string8(isolate, (char *) "key", 0);
         Local<Array> a = DBX_ARRAY_NEW(cx->pqr_next->keyn);
         DBX_SET(obj, key, a);

         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            DBX_SET(a, n, dbx_new_string8n(isolate, cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 0));
         }
         if (cx->getdata) {
            key = dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 0));
         }
      }

      pqr = cx->pqr_next;
      cx->pqr_next = cx->pqr_prev;
      cx->pqr_prev = pqr;

      if (eod == CACHE_SUCCESS) {
         if (cx->format == 1)
            args.GetReturnValue().Set(key);
         else
            args.GetReturnValue().Set(obj);
      }
      else {
         args.GetReturnValue().Set(DBX_NULL());
      }
      return;
   }
   else if (cx->context == 9) {   
      eod = dbx_global_directory(pcon, cx->pqr_prev, 1, &(cx->counter));

      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         key = dbx_new_string8n(isolate, cx->pqr_prev->global_name.buf_addr, cx->pqr_prev->global_name.len_used, c->utf8);
         args.GetReturnValue().Set(key);
      }
      return;
   }
   else if (cx->context == 11) {

      if (!pcon->psql) {
         args.GetReturnValue().Set(DBX_NULL());
         return;
      }

      eod = dbx_sql_row(pcon, pcon->psql->row_no, 1);
      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         int len, dsort, dtype;

         obj = DBX_OBJECT_NEW();

         for (n = 0; n < pcon->psql->no_cols; n ++) {
            len = (int) dbx_get_block_size((unsigned char *) pcon->output_val.svalue.buf_addr, pcon->output_val.offs, &dsort, &dtype);
            pcon->output_val.offs += 5;

            /* printf("\r\n ROW DATA: n=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", n, len, pcon->output_val.offs, dsort, dtype, pcon->output_val.svalue.buf_addr + pcon->output_val.offs); */

            if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
               break;
            }

            key = dbx_new_string8n(isolate, (char *) pcon->psql->cols[n]->name.buf_addr, pcon->psql->cols[n]->name.len_used, 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate,  pcon->output_val.svalue.buf_addr + pcon->output_val.offs, len, 0));
            pcon->output_val.offs += len;
         }

         args.GetReturnValue().Set(obj);
      }
      return;
   }
}


void mcursor::Previous(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int n, eod;
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> key;
   DBXQR *pqr;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->dbx_count ++;

   pcon = c->pcon;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Previous", 1)));
      return;
   }
   if (async) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Cursor based operations cannot be invoked asynchronously", 1)));
      return;
   }

   if (cx->context == 1) {
   
      if (cx->pqr_prev->keyn < 1) {
         args.GetReturnValue().Set(DBX_NULL());
         return;
      }

      DBX_DBFUN_START(c, pcon);
      DBX_DB_LOCK(n, 0);

      eod = dbx_global_order(pcon, cx->pqr_prev, -1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK(n);

      if (cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used == 0) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else if (cx->getdata == 0) {
         key = dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, c->utf8);
         args.GetReturnValue().Set(key);
      }
      else {
         if (cx->format == 1) {
            cx->data.len_used = 0;
            dbx_escape_output(&cx->data, (char *) "key=", 4, 0);
            dbx_escape_output(&cx->data, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, 1);
            dbx_escape_output(&cx->data, (char *) "&data=", 6, 0);
            dbx_escape_output(&cx->data, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 1);
            key = dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
            args.GetReturnValue().Set(key);
         }
         else {
            obj = DBX_OBJECT_NEW();

            key = dbx_new_string8(isolate, (char *) "key", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, c->utf8));
            key = dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 0));
            args.GetReturnValue().Set(obj);
         }
      }
   }
   else if (cx->context == 2) {
   
      DBX_DBFUN_START(c, pcon);
      DBX_DB_LOCK(n, 0);

      eod = dbx_global_query(pcon, cx->pqr_next, cx->pqr_prev, -1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK(n);

      if (cx->format == 1) {
         char buffer[32], delim[4];

         cx->data.len_used = 0;
         *delim = '\0';
         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            sprintf(buffer, (char *) "%skey%d=", delim, n + 1);
            dbx_escape_output(&(cx->data), buffer, (int) strlen(buffer), 0);
            dbx_escape_output(&(cx->data), cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 1);
            strcpy(delim, (char *) "&");
         }
         if (cx->getdata) {
            sprintf(buffer, (char *) "%sdata=", delim);
            dbx_escape_output(&(cx->data), buffer, (int) strlen(buffer), 0);
            dbx_escape_output(&(cx->data), cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 1);
         }

         key = dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
         args.GetReturnValue().Set(key);
      }
      else {
         obj = DBX_OBJECT_NEW();

         key = dbx_new_string8(isolate, (char *) "global", 0);
         DBX_SET(obj, key, dbx_new_string8(isolate, cx->pqr_next->global_name.buf_addr, 0));
         key = dbx_new_string8(isolate, (char *) "key", 0);
         Local<Array> a = DBX_ARRAY_NEW(cx->pqr_next->keyn);
         DBX_SET(obj, key, a);
         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            DBX_SET(a, n, dbx_new_string8n(isolate, cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 0));
         }
         if (cx->getdata) {
            key = dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 0));
         }
      }

      pqr = cx->pqr_next;
      cx->pqr_next = cx->pqr_prev;
      cx->pqr_prev = pqr;

      if (eod == CACHE_SUCCESS) {
         if (cx->format == 1)
            args.GetReturnValue().Set(key);
         else
            args.GetReturnValue().Set(obj);
      }
      else {
         args.GetReturnValue().Set(DBX_NULL());
      }
      return;
   }
   else if (cx->context == 9) {
   
      eod = dbx_global_directory(pcon, cx->pqr_prev, -1, &(cx->counter));

      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         key = dbx_new_string8n(isolate, cx->pqr_prev->global_name.buf_addr, cx->pqr_prev->global_name.len_used, c->utf8);
         args.GetReturnValue().Set(key);
      }
      return;
   }
   else if (cx->context == 11) {

      if (!pcon->psql) {
         args.GetReturnValue().Set(DBX_NULL());
         return;
      }

      eod = dbx_sql_row(pcon, pcon->psql->row_no, -1);
      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         int len, dsort, dtype;

         obj = DBX_OBJECT_NEW();

         for (n = 0; n < pcon->psql->no_cols; n ++) {
            len = (int) dbx_get_block_size((unsigned char *) pcon->output_val.svalue.buf_addr, pcon->output_val.offs, &dsort, &dtype);
            pcon->output_val.offs += 5;

            /* printf("\r\n ROW DATA: n=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", n, len, pcon->output_val.offs, dsort, dtype, pcon->output_val.svalue.buf_addr + pcon->output_val.offs); */

            if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
               break;
            }

            key = dbx_new_string8n(isolate, (char *) pcon->psql->cols[n]->name.buf_addr, pcon->psql->cols[n]->name.len_used, 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate,  pcon->output_val.svalue.buf_addr + pcon->output_val.offs, len, 0));
            pcon->output_val.offs += len;
         }
         args.GetReturnValue().Set(obj);
      }
      return;
   }
}


void mcursor::Reset(const FunctionCallbackInfo<Value>& args)
{
   int rc;
   DBXCON *pcon;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ISOLATE;
   cx->dbx_count ++;

   pcon = c->pcon;
   pcon->argc = args.Length();

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobalquery.reset() method takes at least one argument (the global reference to start with)", 1)));
      return;
   }

   /* 1.4.10 */
   rc = dbx_cursor_reset(args, isolate, (void *) cx, 0, 0);
   if (rc < 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobalquery.reset() method takes at least one argument (the global reference to start with)", 1)));
      return;
   }

   return;
}


void mcursor::Close(const FunctionCallbackInfo<Value>& args)
{
   DBXCON *pcon;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ISOLATE;
   cx->dbx_count ++;

   pcon = c->pcon;
   pcon->argc = args.Length();

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments", 1)));
      return;
   }
   if (pcon->argc > 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Closing a globalquery template does not take any arguments", 1)));
      return;
   }

   if (cx->pqr_next) {
      if (cx->pqr_next->data.svalue.buf_addr) {
         dbx_free((void *) cx->pqr_next->data.svalue.buf_addr, 0);
         cx->pqr_next->data.svalue.buf_addr = NULL;
         cx->pqr_next->data.svalue.len_alloc = 0;
         cx->pqr_next->data.svalue.len_used = 0;
      }
      dbx_free((void *) cx->pqr_next, 0);
      cx->pqr_next = NULL;
   }

   if (cx->pqr_prev) {
      if (cx->pqr_prev->data.svalue.buf_addr) {
         dbx_free((void *) cx->pqr_prev->data.svalue.buf_addr, 0);
         cx->pqr_prev->data.svalue.buf_addr = NULL;
         cx->pqr_prev->data.svalue.len_alloc = 0;
         cx->pqr_prev->data.svalue.len_used = 0;
      }
      dbx_free((void *) cx->pqr_prev, 0);
      cx->pqr_prev = NULL;
   }

/*
   cx->delete_mcursor_template(cx);
*/

   return;
}



