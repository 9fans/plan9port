cd lib9
9c  _exits.c
9c  _p9dialparse.c
9c  _p9dir.c
9c  announce.c
9c  argv0.c
9c  atexit.c
9c  atoi.c
9c  atol.c
9c  atoll.c
9c  atnotify.c
9c  await.c
9c  cistrcmp.c
9c  cistrncmp.c
9c  cistrstr.c
9c  cleanname.c
9c  convD2M.c
9c  convM2D.c
9c  convM2S.c
9c  convS2M.c
9c  create.c
9c  ctime.c
9c  date.c
9c  dial.c
9c  dirfstat.c
9c  dirfwstat.c
9c  dirmodefmt.c
9c  dirread.c
9c  dirstat.c
9c  dirwstat.c
9c  dup.c
9c  encodefmt.c
9c  errstr.c
9c  exec.c
9c  fcallfmt.c
9c  get9root.c
9c getcallerpc-$OBJTYPE.c || 9a getcallerpc-$OBJTYPE.s
9c  getenv.c
9c  getfields.c
9c  getns.c
9c  getuser.c
9c  getwd.c
9c  jmp.c
9c  lrand.c
9c  lnrand.c
9c  main.c
9c  malloc.c
9c  malloctag.c
9c  mallocz.c
9c  nan.c
9c  needsrcquote.c
9c  needstack.c
9c  netmkaddr.c
9c  notify.c
9c  nrand.c
9c  nulldir.c
9c  open.c
9c  opentemp.c
9c  pipe.c
9c  post9p.c
9c  postnote.c
9c  qlock.c
9c  quote.c
9c  read9pmsg.c
9c  readn.c
9c  rfork.c
9c  seek.c
9c  sendfd.c
9c  sleep.c
9c  strdup.c
9c  strecpy.c
9c  sysfatal.c
9c  sysname.c
9c  time.c
9c  tokenize.c
9c  truerand.c
9c  u16.c
9c  u32.c
9c  u64.c
9c  unsharp.c
9c  wait.c
9c  waitpid.c
9c  -Ifmt fmt/dofmt.c
9c  -Ifmt fmt/fltfmt.c
9c  -Ifmt fmt/fmt.c
9c  -Ifmt fmt/fmtfd.c
9c  -Ifmt fmt/fmtfdflush.c
9c  fmtlock2.c
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
9ar rvc $PLAN9/lib/lib9.a *.o
cd ..
cd libbio
9c  bbuffered.c
9c  bfildes.c
9c  bflush.c
9c  bgetc.c
9c  bgetrune.c
9c  bgetd.c
9c  binit.c
9c  boffset.c
9c  bprint.c
9c  bputc.c
9c  bputrune.c
9c  brdline.c
9c  brdstr.c
9c  bread.c
9c  bseek.c
9c  bvprint.c
9c  bwrite.c
9ar rvc $PLAN9/lib/libbio.a *.o
cd ..
cd libregexp
9c  regcomp.c
9c  regerror.c
9c  regexec.c
9c  regsub.c
9c  regaux.c
9c  rregexec.c
9c  rregsub.c
9ar rvc $PLAN9/lib/libregexp9.a *.o
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
9l -o o.mk arc.o archive.o bufblock.o env.o file.o graph.o job.o lex.o main.o match.o mk.o parse.o recipe.o rule.o run.o sh.o shprint.o symtab.o var.o varsub.o word.o unix.o $PLAN9/lib/libregexp9.a $PLAN9/lib/libbio.a $PLAN9/lib/lib9.a 
install -c o.mk $PLAN9/bin/mk
cd ..
