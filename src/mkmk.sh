cd lib9
9c  -Ifmt fmt/dofmt.c
9c  -Ifmt fmt/errfmt.c
9c  -Ifmt fmt/fltfmt.c
9c  -Ifmt fmt/fmt.c
9c  -Ifmt fmt/fmtfd.c
9c  -Ifmt fmt/fmtfdflush.c
9c  -Ifmt fmt/fmtlock.c
9c  -Ifmt fmt/fmtprint.c
9c  -Ifmt fmt/fmtquote.c
9c  -Ifmt fmt/fmtrune.c
9c  -Ifmt fmt/fmtstr.c
9c  -Ifmt fmt/fmtvprint.c
9c  -Ifmt fmt/fprint.c
9c  -Ifmt fmt/nan64.c
9c  -Ifmt fmt/print.c
9c  -Ifmt fmt/runefmtstr.c
9c  -Ifmt fmt/runeseprint.c
9c  -Ifmt fmt/runesmprint.c
9c  -Ifmt fmt/runesnprint.c
9c  -Ifmt fmt/runesprint.c
9c  -Ifmt fmt/runevseprint.c
9c  -Ifmt fmt/runevsmprint.c
9c  -Ifmt fmt/runevsnprint.c
9c  -Ifmt fmt/seprint.c
9c  -Ifmt fmt/smprint.c
9c  -Ifmt fmt/snprint.c
9c  -Ifmt fmt/sprint.c
9c  -Ifmt fmt/strtod.c
9c  -Ifmt fmt/vfprint.c
9c  -Ifmt fmt/vseprint.c
9c  -Ifmt fmt/vsmprint.c
9c  -Ifmt fmt/vsnprint.c
9c  -Ifmt fmt/charstod.c
9c  -Ifmt fmt/pow10.c
9c  utf/rune.c
9c  utf/runestrcat.c
9c  utf/runestrchr.c
9c  utf/runestrcmp.c
9c  utf/runestrcpy.c
9c  utf/runestrdup.c
9c  utf/runestrlen.c
9c  utf/runestrecpy.c
9c  utf/runestrncat.c
9c  utf/runestrncmp.c
9c  utf/runestrncpy.c
9c  utf/runestrrchr.c
9c  utf/runestrstr.c
9c  utf/runetype.c
9c  utf/utfecpy.c
9c  utf/utflen.c
9c  utf/utfnlen.c
9c  utf/utfrrune.c
9c  utf/utfrune.c
9c  utf/utfutf.c
9ar rvc ../../lib/lib9.a dofmt.o errfmt.o fltfmt.o fmt.o fmtfd.o fmtfdflush.o fmtlock.o fmtprint.o fmtquote.o fmtrune.o fmtstr.o fmtvprint.o fprint.o nan64.o print.o runefmtstr.o runeseprint.o runesmprint.o runesnprint.o runesprint.o runevseprint.o runevsmprint.o runevsnprint.o seprint.o smprint.o snprint.o sprint.o strtod.o vfprint.o vseprint.o vsmprint.o vsnprint.o charstod.o pow10.o rune.o runestrcat.o runestrchr.o runestrcmp.o runestrcpy.o runestrdup.o runestrlen.o runestrecpy.o runestrncat.o runestrncmp.o runestrncpy.o runestrrchr.o runestrstr.o runetype.o utfecpy.o utflen.o utfnlen.o utfrrune.o utfrune.o utfutf.o
cd ..
cd libbio
9c  bbuffered.c
9c  bfildes.c
9c  bflush.c
9c  bfmt.c
9c  bgetc.c
9c  bgetd.c
9c  binit.c
9c  boffset.c
9c  bprint.c
9c  bputc.c
9c  brdline.c
9c  brdstr.c
9c  bread.c
9c  bseek.c
9c  bwrite.c
9c  bgetrune.c
9c  bputrune.c
9ar rvc ../../lib/libbio.a bbuffered.o bfildes.o bflush.o bfmt.o bgetc.o bgetd.o binit.o boffset.o bprint.o bputc.o brdline.o brdstr.o bread.o bseek.o bwrite.o bgetrune.o bputrune.o
cd ..
cd libregexp
9c  regcomp.c
9c  regerror.c
9c  regexec.c
9c  regsub.c
9c  regaux.c
9c  rregaux.c
9c  rregexec.c
9c  rregsub.c
9ar rvc ../../lib/libregexp9.a regcomp.o regerror.o regexec.o regsub.o regaux.o rregaux.o rregexec.o rregsub.o
cd ..
cd cmd/mk
9c  arc.c
9c  archive.c
9c  bufblock.c
9c  env.c
9c  file.c
9c  graph.c
9c  job.c
9c  lex.c
9c  main.c
9c  match.c
9c  mk.c
9c  parse.c
9c  recipe.c
9c  rule.c
9c  run.c
9c  sh.c
9c  shprint.c
9c  symtab.c
9c  var.c
9c  varsub.c
9c  word.c
9c  unix.c
9l -o o.mk arc.o archive.o bufblock.o env.o file.o graph.o job.o lex.o main.o match.o mk.o parse.o recipe.o rule.o run.o sh.o shprint.o symtab.o var.o varsub.o word.o unix.o -lregexp9 -lbio -l9
install -c o.mk ../../../bin/mk
cd ..
