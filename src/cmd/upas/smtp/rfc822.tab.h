/* A Bison parser, made by GNU Bison 2.0.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     WORD = 258,
     DATE = 259,
     RESENT_DATE = 260,
     RETURN_PATH = 261,
     FROM = 262,
     SENDER = 263,
     REPLY_TO = 264,
     RESENT_FROM = 265,
     RESENT_SENDER = 266,
     RESENT_REPLY_TO = 267,
     SUBJECT = 268,
     TO = 269,
     CC = 270,
     BCC = 271,
     RESENT_TO = 272,
     RESENT_CC = 273,
     RESENT_BCC = 274,
     REMOTE = 275,
     PRECEDENCE = 276,
     MIMEVERSION = 277,
     CONTENTTYPE = 278,
     MESSAGEID = 279,
     RECEIVED = 280,
     MAILER = 281,
     BADTOKEN = 282
   };
#endif
#define WORD 258
#define DATE 259
#define RESENT_DATE 260
#define RETURN_PATH 261
#define FROM 262
#define SENDER 263
#define REPLY_TO 264
#define RESENT_FROM 265
#define RESENT_SENDER 266
#define RESENT_REPLY_TO 267
#define SUBJECT 268
#define TO 269
#define CC 270
#define BCC 271
#define RESENT_TO 272
#define RESENT_CC 273
#define RESENT_BCC 274
#define REMOTE 275
#define PRECEDENCE 276
#define MIMEVERSION 277
#define CONTENTTYPE 278
#define MESSAGEID 279
#define RECEIVED 280
#define MAILER 281
#define BADTOKEN 282




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



