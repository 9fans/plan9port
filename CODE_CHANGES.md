# CODE_CHANGES.md — La fonctionnalité d'emphase (`Emph`) dans acme

Ce document explique, pour un développeur qui découvre le code, l'ensemble des
changements apportés par rapport au dépôt en amont. Il décrit *ce qui* a été ajouté,
*pourquoi*, et *les difficultés* rencontrées en chemin.

---

## 1. Que fait la nouvelle fonctionnalité ?

On ajoute à l'éditeur `acme` la possibilité de **mettre en évidence** (« emphasis »)
toutes les occurrences d'une expression régulière dans le corps d'une fenêtre, en
les rendant dans une **police différente** et une **couleur différente**.

Deux façons de l'utiliser :

1. **Commande utilisateur** `Emph` — clic bouton-2 sur le mot `Emph` dans un tag.
   - `Emph foo` : emphase toutes les occurrences de `foo`.
   - `Emph` (sans argument) : bascule l'emphase active (on/off).
   - Un « chord » 2-1 fournit l'argument, comme pour `Look`.
2. **Contrôle externe via 9P** — acme expose chaque fenêtre comme un système de
   fichiers ; on écrit dans le fichier `ctl` :
   - `emph=regex` : active l'emphase.
   - `noemph` : la désactive (le regex reste mémorisé).

La police d'emphase se configure au lancement : `-e` (police à chasse variable),
`-E` (police à chasse fixe). La couleur d'emphase se configure avec `-C colorspec`
(3 ou 6 chiffres hexadécimaux RGB, ex: `82f` ou `8822ff`).

---

## 2. Le problème central : libframe ne connaît qu'une seule police

Le texte d'une fenêtre acme est dessiné par une bibliothèque appelée **libframe**
(`src/libframe/`). libframe découpe le texte visible en **boîtes** (`Frbox`) — une
boîte est un fragment de texte de même nature (un mot, une tabulation, un saut de
ligne). Pour dessiner, mesurer et localiser le texte, libframe utilisait *toujours*
une unique police : `frame->font`.

C'est l'obstacle fondamental : pour emphaser, il faut que **certaines boîtes**
soient rendues dans une police et **d'autres** dans une autre. libframe était par
construction « mono-police ».

Une première tentative (abandonnée, voir le plan) consistait à **superposer** un
second passage de dessin par-dessus le rendu standard. Cette approche « overlay »
a produit cinq bugs structurels (sélection souris incohérente, artefacts de
pixels, emphase fantôme après changement de regex, crashs). Ces bugs étaient
**insolubles** tant que libframe restait mono-police.

**La décision** : étendre libframe pour qu'une boîte puisse porter sa *propre*
police. La politique (« quelles plages emphaser ») reste dans acme ; libframe ne
gagne que la *capacité* technique de rendre boîte par boîte.

---

## 3. Architecture en deux couches

```
   COUCHE ACME (la politique : quoi emphaser, quelle couleur)
   --------------------------------------------------------------
   commande Emph / ctl emph=        ->  setemph()
   emphmatch[] : liste triée des plages qui matchent le regex
   emphapply() : projette emphmatch[] sur les boîtes visibles
                          |
                          v   appelle frsetboxfont()
   --------------------------------------------------------------
   COUCHE LIBFRAME (le mécanisme : rendre une boîte autrement)
   --------------------------------------------------------------
   Frbox.font : police propre à la boîte (nil => police du frame)
   Frame.cols[EMPH] : couleur de premier plan pour texte emphasé
   FRBOXFONT(f,b) : macro qui choisit la bonne police
   frsetboxfont() : affecte une police à une plage de boîtes
   _frdrawtext() / frdrawsel0() : utilisent cols[EMPH] si b->font != nil
```

Point clé de **rétrocompatibilité** : une boîte dont `font == nil` se comporte
exactement comme avant (`FRBOXFONT` retombe sur `frame->font`, couleur sur `TEXT`).
Les autres programmes qui utilisent libframe (`sam`, `samterm`, `9term`) n'appellent
jamais `frsetboxfont` — ils ne voient donc aucun changement (ils initialisent
`cols[EMPH]` à noir par précaution).

---

## 4. Couche libframe — la police par boîte

### `include/frame.h`

- Ajout d'un champ `Font *font` dans la structure `Frbox` (commentaire :
  `nil => use frame->font`).
- Ajout de la macro centrale :
  ```c
  #define FRBOXFONT(f,b)  ((b)->font ? (b)->font : (f)->font)
  ```
  Toute la stratégie tient dans cette ligne : « la police de la boîte si elle en
  a une, sinon celle du frame ».
- Déclaration de la nouvelle fonction publique `frsetboxfont`.

### `src/libframe/frboxfont.c` *(nouveau fichier)*

Contient `frsetboxfont(Frame *f, ulong p0, ulong p1, Font *font)` : affecte
`font` à toutes les boîtes couvrant la plage de caractères `[p0, p1)`.

Difficulté résolue ici : une plage `[p0, p1)` ne coïncide pas forcément avec des
frontières de boîtes. La fonction appelle donc `_frfindbox` qui **découpe** les
boîtes aux positions `p0` et `p1` avant de parcourir et d'affecter la police.
Lorsqu'une boîte change de police, sa largeur (`b->wid`) est recalculée dans la
nouvelle police via `stringnwidth`.

### `src/libframe/frdraw.c` — le dessin

Deux endroits dessinaient le texte avec `f->font` ; remplacés par
`FRBOXFONT(f, b)` :
- `_frdrawtext` (dessin normal du texte).
- `frdrawsel0` (dessin avec sélection — mesure *et* dessin doivent utiliser la
  même police, sinon le rectangle d'effacement ne couvre pas le glyphe).

**Extension ultérieure (2026-05-15)** : ces deux fonctions ont été modifiées pour
utiliser `f->cols[EMPH]` au lieu de `f->cols[TEXT]` lorsque `b->font != nil`,
permettant ainsi de colorer le texte emphasé différemment (voir § 5.1).

### `src/libframe/frutil.c` — la mise en page

- `_frcanfit` (combien de runes tiennent sur une ligne) : la mesure rune par rune
  doit utiliser la police de la boîte.
- `_frclean` (fusion des boîtes adjacentes pour économiser de la mémoire) : on
  **interdit** désormais de fusionner deux boîtes de polices différentes
  (`b[0].font == b[1].font` ajouté à la condition). Sans cette garde, une boîte
  emphasée et une boîte normale fusionneraient et perdraient leur distinction.

### `src/libframe/frbox.c` — le redimensionnement des boîtes

`truncatebox` et `chopbox` recalculaient une largeur avec `f->font` ; passés à
`FRBOXFONT(f, b)` pour rester cohérents quand on coupe une boîte emphasée.

### `src/libframe/frptofchar.c` — la conversion pixel ↔ caractère

`_frptofcharptb` et `frcharofpt` traduisent une position souris en index de
caractère (et inversement) en avançant rune par rune. Ils utilisaient `f->font`.

> **Bug subtil** : si une boîte emphasée est *plus large* que la même chaîne en
> police standard, la marche rune par rune en police standard n'atteignait jamais
> `b->wid`. La boucle de `frcharofpt` consommait toute la chaîne, arrivait à la
> fin et appelait `drawerror("end of string in frcharofpt")` — ce qui **fait
> planter acme**. C'était la cause des crashs « occasionnels » au clic souris sur
> du texte emphasé (occasionnels car déclenchés seulement si le clic tombe dans
> la zone de débordement).

Correctif : les deux `f->font` deviennent `FRBOXFONT(f, b)`.

### `src/libframe/frinsert.c` — la création de boîtes

`bxscan` crée les nouvelles boîtes. On initialise explicitement `b->font = nil` :
une boîte fraîchement insérée est par défaut sans emphase ; acme appliquera la
police après coup si nécessaire.

> **Leçon** : un champ ajouté à une structure n'est pas zéro-initialisé partout.
> Toute fabrique de `Frbox` doit penser à mettre `font = nil`.

---

## 5. Couche acme — la politique d'emphase

### `src/cmd/acme/acme.c` — options de ligne de commande

- Le tableau `fontnames` passe de 2 à 4 entrées : `[0]`/`[1]` sont les polices
  variable/fixe habituelles, `[2]`/`[3]` les polices d'emphase variable/fixe.
- Nouvelles options `-e` et `-E` qui remplissent `fontnames[2]`/`[3]`.
- La variable d'environnement `$emphfont` est exportée.
- Message d'usage mis à jour.

### `src/cmd/acme/dat.h` — l'état persistant par fenêtre

Nouveaux champs dans `struct Window` :

| Champ | Rôle |
|---|---|
| `emphon` | l'emphase est-elle actuellement affichée ? |
| `emphpat` / `nemphpat` | le dernier regex compilé (chaîne de runes) |
| `emphmatch` | **tableau trié, sans chevauchement, des plages qui matchent** — c'est la source de vérité |
| `nemphmatch` / `aemphmatch` | nombre d'éléments utilisés / capacité allouée |
| `emphfont` | la police d'emphase de la fenêtre (chargée à la première utilisation) |

Idée importante : `emphmatch[]` stocke des **offsets dans le fichier**, pas des
positions à l'écran. Ces offsets survivent au défilement ; ce sont les boîtes du
frame qui sont volatiles.

### `src/cmd/acme/emphranges.c` *(nouveau fichier)* — la logique pure

Quatre fonctions sur des tableaux de `Range`, **sans aucune dépendance** à acme ou
à l'affichage (ce qui les rend faciles à tester unitairement) :

- `emphpush` — ajoute une plage, agrandit le tableau en doublant sa capacité.
- `rangeshift` — décale les plages après une insertion/suppression de texte ;
  **abandonne** les plages qui chevauchent l'édition (elles ne sont plus valides).
- `rangemerge` — fusionne deux tableaux triés en gardant l'ordre.
- `emphfreearr` — libère le tableau.

### `src/cmd/acme/text.c` — le cœur du rendu

Nouvelles fonctions :

- **`emphclear`** — remet *toutes* les boîtes du frame en police standard
  (`frsetboxfont(f, 0, nchars, nil)`).
- **`emphapply`** — la fonction pivot. Efface tout (`emphclear`), puis pour chaque
  plage de `emphmatch[]` *visible* à l'écran, applique la police d'emphase aux
  boîtes correspondantes. Convertit les offsets fichier en offsets frame en
  tenant compte de `body.org` (le décalage de défilement).
- **`setemph`** — point d'entrée unique. Branche « off » : éteint et redessine.
  Branche « on » : compile le regex, charge la police à la demande, recalcule
  `emphmatch[]`, applique, redessine.
- **`emphrecompute`** — recalcule `emphmatch[]` *entièrement* en balayant tout le
  fichier avec `rxexecute`.
- **`emphrefreshlocal`** — recalcul *local* : ne réexamine qu'une fenêtre
  `[q0-200, q1+200]` autour d'une édition, pour ne pas rebalayer tout le fichier
  à chaque frappe.
- **`emphshift`** — après une édition, décale les plages via `rangeshift`.
- **`emphfree`** — libère tout l'état d'emphase (appelé à la fermeture de fenêtre).
- **`emphfontname`** — choisit `fontnames[3]` (police d'emphase à chasse fixe) si
  le corps de la fenêtre est rendu en police fixe, sinon `fontnames[2]` (variable).
  La comparaison se fait sur le nom de police du corps (`w->body.fr.font->name`).
- **`emphfontupdate`** — recharge `w->emphfont` pour qu'elle suive le mode du corps
  (variable/fixe), puis ré-applique l'emphase. Appelée par `fontx` (`exec.c`) quand
  la commande `Font` bascule la police du corps.

> **Détail des boucles de matching** : après chaque match, on avance
> `p = q1` ; mais si le match est *vide* (`q0 == q1`, cas des ancres `^` ou `$`),
> on avance `p = q0 + 1` pour éviter une **boucle infinie**.

Trois « hooks » branchés dans les fonctions existantes :

- `textinsert` / `textdelete` : après modification du texte, `emphshift` +
  `emphrefreshlocal` + `emphapplylocal` mettent l'emphase à jour.
- `textredraw` et `textsetorigin` : après remplissage du frame, `emphapply` +
  `frredraw` — sinon l'emphase disparaît au défilement et au déplacement de
  fenêtre (les boîtes sont reconstruites avec `font = nil` à chaque scroll).

### `src/cmd/acme/exec.c` — la commande `Emph`

- Ajout de `LEmph` et de l'entrée correspondante dans `exectab[]` (la table qui
  associe un nom de commande à son handler).
- Le handler `emph` gère : argument direct, argument par chord, bascule on/off,
  réactivation du dernier regex.
- Le handler `get` (commande `Get`, recharge le fichier depuis le disque) appelle
  désormais `emphrecompute` + `emphapply` + `frredraw` : sans cela l'emphase
  disparaissait après un `Get`.
- Le handler `fontx` (commande `Font`) appelle `emphfontupdate` après avoir changé
  la police du corps : la commande `Font` bascule alors *à la fois* la police du
  corps et celle d'emphase (variable <-> fixe).

> **Bug évité — l'auto-blocage (deadlock)** : une première version verrouillait la
> fenêtre avec `winlock(w, 'E')` dans le handler. Mais `winlock` repose sur
> `qlock`, qui **n'est pas réentrant**, et l'appelant détient *déjà* le verrou
> (via le chemin souris ou le chemin 9P). Reprendre le verrou gelait acme. La
> version finale ne verrouille pas — la sérialisation est garantie par l'appelant ;
> un commentaire dans le code l'explique.

### `src/cmd/acme/xfid.c` — le contrôle via 9P

Dans `xfidctlwrite` (qui traite ligne par ligne les écritures dans le fichier
`ctl`), deux nouveaux verbes :
- `emph=regex` — convertit le regex en runes (`cvttorunes`), refuse les octets
  nuls, appelle `setemph(..., TRUE)`.
- `noemph` — appelle `setemph(..., FALSE)`.

### `src/cmd/acme/wind.c` — cycle de vie de la fenêtre

- `wininit` : initialise les champs d'emphase à zéro/`nil`.
- `winclose` : appelle `emphfree` pour libérer le regex, le tableau de plages et
  la police (`rfclose`).

---

## 5.1. Extension : couleur d'emphase (ajout 2026-05-15)

Après l'implémentation de la police par boîte, une extension naturelle consiste à
permettre de **changer la couleur** du texte emphasé. Par défaut, le texte emphasé
était dessiné en noir (comme le texte normal), seule la police changeait.

### Le problème : libframe « mono-couleur » par fonction

Les fonctions de dessin de libframe (`_frdrawtext`, `frdrawsel0`) recevaient une
couleur de texte en paramètre (`Image *text`) et l'utilisaient pour *toutes* les
boîtes. Pour colorer différemment le texte emphasé, il faut que libframe puisse
choisir une couleur différente selon la boîte.

### La solution : extension du système de couleurs

**Décision architecturale** : étendre l'enum des couleurs de `Frame.cols[]` pour
ajouter un slot `EMPH` dédié au texte emphasé. C'est cohérent avec l'approche
« police par boîte » : libframe fournit le *mécanisme* (capacité de rendre avec
une couleur différente), acme fournit la *politique* (quelle couleur utiliser).

### `include/frame.h` — extension de l'enum des couleurs

L'enum des couleurs passe de 5 à 6 entrées :
```c
enum{
    BACK,   // fond
    HIGH,   // fond de sélection
    BORD,   // bordure
    TEXT,   // texte normal
    HTEXT,  // texte sélectionné
    EMPH,   // texte emphasé (nouveau)
    NCOL
};
```

`NCOL` passe de 5 à 6. Tous les programmes qui utilisent libframe (`acme`, `sam`,
`samterm`, `9term`) doivent initialiser ce nouveau slot.

### `src/libframe/frdraw.c` — utilisation de la couleur d'emphase

Deux fonctions de dessin modifiées :

- **`_frdrawtext`** : au lieu d'utiliser toujours le paramètre `text`, on choisit
  `f->cols[EMPH]` si la boîte a une police personnalisée (`b->font != nil`).
  ```c
  col = (b->font != nil) ? f->cols[EMPH] : text;
  stringbg(f->b, pt, col, ZP, FRBOXFONT(f, b), (char*)b->ptr, back, ZP);
  ```

- **`frdrawsel0`** : même logique pour le dessin avec sélection.
  ```c
  col = (b->font != nil) ? f->cols[EMPH] : text;
  stringnbg(f->b, pt, col, ZP, FRBOXFONT(f, b), ptr, nr, back, ZP);
  ```

Le test `b->font != nil` sert de **marqueur** : une boîte avec police personnalisée
est une boîte emphasée, donc elle doit être dessinée avec la couleur d'emphase.

### `src/cmd/acme/acme.c` — option `-C` et parsing de couleur

Nouvelle option de ligne de commande `-C colorspec` :
- `colorspec` : 3 ou 6 chiffres hexadécimaux RGB.
- 3 chiffres : chaque chiffre est doublé (ex: `82f` → `0x8822ff`).
- 6 chiffres : utilisé tel quel (ex: `8822ff` → `0x8822ff`).

Variable globale `emphcolorspec` (déclarée dans `dat.h`, définie dans `dat.c`)
stocke la chaîne fournie par l'utilisateur.

Fonction `iconinit()` modifiée pour :
1. Parser `emphcolorspec` via `parsecolor()` (voir ci-dessous).
2. Créer une `Image` de couleur via `allocimage()`.
3. Affecter cette image à `tagcols[EMPH]` et `textcols[EMPH]`.
4. Si `-C` n'est pas fourni, utiliser une couleur par défaut (bleu foncé `0x0000AAFF`).

### `src/cmd/acme/util.c` — fonction `parsecolor`

Nouvelle fonction utilitaire :
```c
int parsecolor(char *spec, ulong *rgb)
```

Parsing :
- Vérifie que `spec` contient 3 ou 6 caractères hexadécimaux.
- Convertit en valeur numérique.
- Si 3 chiffres : expansion par doublement de chaque chiffre.
  ```
  0x82f → 0x8 0x2 0xf
       → 0x88 0x22 0xff
       → 0x8822ff
  ```
- Ajoute le canal alpha (`0xFF`) pour obtenir un RGBA 32 bits.
- Retourne 0 en cas de succès, -1 en cas d'erreur.

Prototype ajouté dans `fns.h`.

### Rétrocompatibilité avec sam, samterm, 9term

Ces programmes utilisent libframe mais n'ont pas besoin de la fonctionnalité
d'emphase. Pour qu'ils compilent avec `NCOL=6`, on initialise `cols[EMPH]` à
`display->black` dans leurs fonctions d'initialisation :

- `src/cmd/samterm/flayer.c` : `flstart()` initialise `maincols[EMPH]` et `cmdcols[EMPH]`.
- `src/cmd/9term/wind.c` : `wmk()` initialise `cols[EMPH]`.

Comme ces programmes n'appellent jamais `frsetboxfont`, aucune boîte n'aura
`font != nil`, donc `cols[EMPH]` ne sera jamais utilisé. L'initialisation à noir
est purement défensive.

### Documentation

- `man/man1/acme.1` : ajout de l'option `-C colorspec` dans la section SYNOPSIS
  et description du format dans la section OPTIONS.
- Pas de changement dans `man/man4/acme.4` : la couleur est une option CLI, pas
  un verbe `ctl`.

### Pourquoi cette approche ?

**Alternative envisagée** : passer la couleur d'emphase en paramètre aux fonctions
de dessin. Rejeté car :
- Plus invasif (changement de signature de fonctions publiques).
- Moins cohérent avec l'architecture existante (`Frame.cols[]` centralise déjà
  toutes les couleurs).

**Choix retenu** : extension de `Frame.cols[]`. Avantages :
- Cohérent avec le système de couleurs existant.
- Minimal : une seule entrée ajoutée à l'enum, deux lignes de code dans les
  fonctions de dessin.
- Rétrocompatible : les programmes qui n'utilisent pas l'emphase initialisent
  simplement le slot supplémentaire.

---

## 5.2. Extension : hauteur de ligne variable (Phase 1, 2026-05-15)

Jusqu'à la Phase 1, libframe supposait que toutes les boîtes d'un frame ont la
même hauteur — celle de `f->font`. Avec une police d'emphase de hauteur différente,
ce supposé était violé, produisant des lignes trop courtes, une mauvaise détection
de clic, et un curseur de saisie trop petit.

### Nouveaux champs dans `Frame`

```c
int  lineheight;   /* hauteur de ligne effective (>= font->height) */
int  ascent;       /* distance haut-de-ligne -> baseline */
```

`frinit` initialise ces champs à `ft->height` et `ft->ascent`. Tous les calculs
de hauteur dans libframe utilisent désormais `f->lineheight` à la place de
`f->font->height`.

### Macro `FRBOXDY`

```c
#define FRBOXDY(f,b)  ((f)->ascent - FRBOXFONT(f,b)->ascent)
```

Cette macro calcule le décalage vertical pour aligner le texte d'une boîte sur la
baseline commune du frame. Pour une boîte standard (`b->font == nil`) avec une
police de même ascent que le frame, `FRBOXDY` vaut 0 — comportement identique à
l'existant (rétrocompatibilité `sam`/`samterm`/`9term`).

### Côté acme : `emphsetmetrics`

Appelé juste après `frinit` dans `textredraw`, ce helper calcule
`max(font->height, emphfont->height)` et `max(font->ascent, emphfont->ascent)`
et met à jour `fr.lineheight` / `fr.ascent`. Il requantifie ensuite `fr.r.max.y`
et `fr.maxlines` à partir de `fr.entire` (non quantifié par `frsetrects`), puis
appelle `frinittick` pour redimensionner le curseur.

**Décision de conception** : la hauteur de ligne d'un body est fixée dès que la
fenêtre possède une police d'emphase chargée, indépendamment de `emphon`. Activer
ou désactiver l'emphase ne provoque pas de recomposition — seul le chargement ou
le changement de police d'emphase le fait (via `winresize` dans `winensureemphfont`).

---

## 5.3. Commande `EmphFont` + fichier 9P `nctl` (Phase 2, 2026-05-15)

### Champ `Window.emphfontpath`

Nouveau champ `char *emphfontpath` dans `struct Window` : chemin de la police
d'emphase épinglée par `EmphFont`. Nil = choix automatique selon `emphfontname`.

### `winensureemphfont`

Centralise l'acquisition de la police d'emphase : choisit `emphfontpath` s'il est
non-nil, sinon `emphfontname(w)`. Si la police change (nouvelle hauteur possible),
appelle `rfclose` puis `winresize(w, w->r, FALSE, TRUE)` pour recomposer.

### Commande `EmphFont` (`exec.c`)

Enregistrée dans `exectab[]` (entre `Emph` et `Exit`). Handler `emphfontx` :
argument direct ou chord 2-1, stocke le chemin dans `w->emphfontpath`, appelle
`winensureemphfont`, puis ré-applique l'emphase. Sans argument : reset au choix
automatique.

### Fichier 9P `nctl`

Nouveau fichier par fenêtre dans `dirtabw[]` (QWnctl, entre `event` et `rdsel`).

- **Lecture** : renvoie le nom de la police d'emphase courante + `\n`, ou `\n`
  seul si aucune police n'est chargée.
- **Écriture** : verbes `emphfont <path>`, `emphfont` (reset), `emph=<regex>`,
  `noemph`. Traités par `xfidnctlwrite`.

Les verbes `emph=` et `noemph` ont été **retirés de `ctl`** pour que `ctl` reste
strictement standard. Toute écriture d'emphase doit désormais passer par `nctl`.

---

## 5.4. Persistance dump/load (Phase 3, 2026-05-15)

### Bloc emphase `m` par fenêtre

`rowdump1` émet, après le bloc corps/dumpstr de chaque fenêtre, trois lignes
supplémentaires si la fenêtre a une emphase active :

```
m <emphon>
<emphfontpath ou ligne vide>
<emphpat en UTF ou ligne vide>
```

`rowload` reconnaît `case 'm'` dans son `switch(l[0])` et restaure `emphfontpath`,
appelle `winensureemphfont`, puis `setemph`. Rétrocompatible : un dump sans ligne
`m` est chargé normalement.

### Ligne `A` globale

`rowdump1` émet `A <autoemph>` en toute fin de fichier si `autoemph != 0`.
`rowload case 'A'` restaure `autoemph = atoi(l+2)`.

---

## 5.5. `lib/emph.regexp` + `EmphMe`/`EmphAll`/`EmphNone`/`AutoEmph` (Phase 4, 2026-05-15)

### `lib/emph.regexp`

Fichier `$PLAN9/lib/emph.regexp` (ou `$HOME/lib/emph.regexp` en repli) : une
ligne `ext=motif` par extension de fichier reconnue (13 langages initiaux : flix,
jl, c, h, py, rs, go, js, md, sh, rc, hs, lua).

### Fonctions de logique commune (`text.c`)

- `fileext(Rune *name, int nname, int *plen)` : extrait l'extension après le
  dernier `.` situé après le dernier `/`.
- `emphpattern(char *ext)` : ouvre `lib/emph.regexp`, cherche `ext=...`, renvoie
  le motif en runes.
- `emphbyext(Window *w)` : combine les deux, appelle `setemph` si le motif est
  trouvé.
- `emphauto(Window *w)` : appelle `emphbyext` si `autoemph && !w->emphon &&
  w->emphpat == nil`.

### Nouvelles commandes (`exec.c`)

| Commande | Comportement |
|---|---|
| `EmphMe` | `emphbyext` sur la fenêtre courante |
| `EmphAll` | `emphbyext` sur toutes les fenêtres non-scratch |
| `EmphNone` | `setemph(w, nil, 0, FALSE)` sur toutes les fenêtres |
| `AutoEmph` | bascule `autoemph` (global), affiche l'état |

### Hook `emphauto`

Appelé à la fin de `get()` (`exec.c`) et dans les trois entonnoirs d'ouverture de
fichier : `readfile()` (`acme.c`), `plumbshow()` et `openfile()` (`look.c`).
`AutoEmph` s'applique donc à l'ouverture CLI, plumb, `New`, et `Get`.

### Variable globale `autoemph`

`int autoemph;` défini dans `dat.c`, déclaré `extern` dans `dat.h`. Persisté par
le dump (ligne `A`, voir § 5.4).

---

## 5.6. Correctif : débordement / crash après `Emph` (2026-05-16)

La police d'emphase est souvent **plus large** que la police standard. Or
`frsetboxfont` (libframe) se contente d'élargir les boîtes emphasées
(`b->wid` recalculé) **sans refaire la mise en page**. Les boîtes dépassent
alors `f->r.max.x` tandis que `nlines`/`maxlines`/`nchars` restent figés sur
la mise en page calculée en police standard.

Conséquences observées :

1. `frdrawsel0`/`_frcklinewrap` replient une boîte **entière** dès qu'elle ne
   tient plus — d'où des plis « étranges » tôt dans une longue ligne, et
   **plus de lignes affichées** que prévu. La zone de corps déborde sur la
   tagline de la fenêtre du dessous.
2. `nlines` périmé (trop petit) rend incohérents les calculs de scroll et de
   conversion pixel ↔ caractère → `drawerror`/`sysfatal` (crash) après
   quelques scrolls et déplacements de fenêtres.

### `src/libframe/frboxfont.c` — nouvelle fonction `frrelayout`

`frrelayout(Frame *f)` rejoue la passe de mise en page **pure** `_frdraw`
(re-découpe les boîtes pour la largeur courante, supprime le trop-plein qui
déborde du frame et ajuste `nchars`), puis `_frclean` (fusion des boîtes
adjacentes, `lastlinefull`), recalcule `nlines` et borne `p0`/`p1` sur
`nchars`. Aucun dessin : l'appelant redessine ensuite via `frredraw`.

Rejouer `_frdraw` sur un tableau déjà découpé est sûr : l'emphase ne fait
qu'élargir, donc les points de repli ne peuvent que reculer.

### `src/cmd/acme/text.c` — branchement

`emphapply` appelle désormais `frrelayout` **dans tous les cas** (emphase on
*et* off), suivi de `textfill` : à l'extinction de l'emphase les boîtes
rétrécissent et le frame sous-rempli est complété (`textfill` s'auto-garde
via `lastlinefull`). `setemph` (et `emphrefresh`) appellent `textsetselect`
avant `frredraw` pour resynchroniser la sélection si `_frdraw` a supprimé des
boîtes.

### `src/cmd/acme/cols.c` — durcissement hauteur

`colresize` utilisait encore le `font->height` global comme hauteur minimale
de fenêtre ; remplacé par `tag.fr.font->height*taglines + body.fr.lineheight`,
de sorte qu'une police d'emphase plus **haute** laisse toujours la place
d'au moins une ligne de corps.

---

## 6. Récapitulatif des difficultés rencontrées

| Problème | Cause | Solution |
|---|---|---|
| libframe mono-police | `f->font` partout | champ `Frbox.font` + macro `FRBOXFONT` |
| Approche « overlay » buggée | second passage de dessin fragile | abandonnée au profit de la police par boîte |
| Crash au clic souris | `frptofchar` marchait en police standard sur des boîtes plus larges | `FRBOXFONT` dans `frptofchar.c` |
| Emphase perdue au scroll / déplacement / `Get` | les boîtes sont reconstruites avec `font = nil` | rejouer `emphapply` dans `textsetorigin`, `textredraw`, `get` |
| Fusion de boîtes incohérente | `_frclean` fusionnait des polices différentes | garde `b[0].font == b[1].font` |
| Auto-blocage de la commande `Emph` | `winlock` non réentrant, déjà tenu par l'appelant | suppression du verrou interne |
| Police d'emphase fixe (`-E`) jamais utilisée | `setemph` chargeait toujours `fontnames[2]` | `emphfontname` choisit `[2]`/`[3]` selon le mode du corps |
| `Font` ne basculait pas l'emphase | `fontx` ignorait `w->emphfont` | `emphfontupdate` appelé après le changement de police |
| Emphase fantôme après collage | `frinsert` repeint les boîtes *avant* que `emphapply` corrige leur police, et `emphapply` seul ne redessine pas | `emphapplylocal` appelle `frredraw` après `emphapply` |
| Débordement / crash après `Emph` (police plus large) | `frsetboxfont` élargit les boîtes sans refaire la mise en page ; `nlines`/`nchars` périmés | `frrelayout` rejoue `_frdraw` après `frsetboxfont`, appelée par `emphapply` ; voir § 5.6 |

Le **dernier** cas mérite un mot, car il illustre un piège d'ordre des opérations.
Lorsqu'on colle un caractère au milieu d'un mot emphasé (`abc` devient `a bc`,
qui ne matche plus `abc`) :

1. `frinsert` repeint les boîtes touchées — mais à cet instant elles portent
   encore l'ancienne police d'emphase, donc elles sont dessinées emphasées.
2. `emphapply` corrige ensuite la *donnée* (`b->font` repasse à `nil`) mais ne
   **repeint pas** l'écran.

Résultat : `a` et `bc` restaient emphasés visuellement. Le correctif : faire
suivre `emphapply` d'un `frredraw` dans `emphapplylocal`, comme le font déjà tous
les autres appelants de `emphapply`.

---

## 7. Tests

- `src/cmd/acme/emphranges.c` est testé par `src/cmd/acme/tests/emph_test.c`
  (44 tests, groupes A-F) — logique pure des plages.
- `src/cmd/acme/tests/g_test.c` (11 tests, G1-G4) — intégration avec le moteur de
  regex `rxcompile` / `rxexecute`, dont les cas délicats : matches vides (`^`,
  `$`) sans boucle infinie.
- `src/libframe/tests/frtest.c` (14 tests) — vérifie `frsetboxfont` sur un frame
  créé en mémoire (sans affichage).

Les tests pure-logique ne couvrent pas le rendu graphique : la vérification du
chemin de dessin se fait à la main en lançant `acme`.

---

## 8. Documentation

- `man/man1/acme.1` : options `-e`/`-E` (sans restriction de hauteur), option
  `-C`, commandes `Emph`, `EmphAll`, `EmphFont`, `EmphMe`, `EmphNone`, `AutoEmph`.
  Fichier `$PLAN9/lib/emph.regexp` dans la section FILES.
- `man/man4/acme.4` : fichier `nctl` (verbes `emphfont path`, `emphfont`, `emph=`,
  `noemph` ; lecture = police courante). Les verbes `emph=`/`noemph` ont été
  **retirés de `ctl`** pour garder `ctl` strictement standard.
- `man/man3/frame.3` : champs `lineheight` et `ascent` de `Frame`, sémantique de
  hauteur variable, slot `EMPH` dans l'enum `cols[]` (NCOL = 6).

---

## 9. Correctifs des bugs d'emphasis (2026-05-16)

Après le déploiement initial, deux bugs critiques ont été identifiés et corrigés :

### Bug 1 — Crash immédiat après toute écriture avec emphasis activée

**Symptôme** : Lors de la frappe avec un EmphFont plus grand, acme plantait
quelques millisecondes après chaque caractère sans message d'erreur.

**Cause racinaire** : `textinsert` appelait `emphapplylocal()` même quand `tofile==FALSE`
(c'est-à-dire quand le texte était juste en cache, pas encore dans le fichier).
`emphapply` utilisait alors des ranges d'emphasis qui ne correspondaient pas au
contenu réel du fichier, ce qui causait un état invalide dans `frrelayout`.

**Correctif** (`text.c:433`) :
```c
// AVANT:
if(t->what == Body && t->w && t->w->emphon){
    emphshift(t->w, q0, n);
    if(tofile)
        emphrefreshlocal(t->w, q0, q0 + n);
    emphapplylocal(t->w, q0, q0 + n);  // BUG: toujours appelé
}

// APRÈS:
if(t->what == Body && t->w && t->w->emphon){
    emphshift(t->w, q0, n);
    // Defer all emphasis refresh/apply to textcommit
}
```

Similaire dans `textdelete` (ligne 553) : rendre `emphshift` seul.

**Résultat** : L'emphasis n'est appliquée/rafraîchie que dans `textcommit`, une
seule fois par opération d'édition, quand le fichier est effectivement à jour.

### Bug 2 — Caractères corrompus et crashes lors de cut/paste massif avec emphasis

**Symptôme** : Après un paste massif (boucle) avec emphasis activée, des caractères
« face » (non-imprimables, génériques) apparaissaient dans la zone collée. Suivi
d'un crash au clic suivant ou au cut suivant.

**Cause racinaire** : `textinsert` appelait `emphapplylocal()` à **chaque itération**
de la boucle paste. Cela modifiait progressivement `emphmatch[]` sans synchronisation :
la première itération supprimait les ranges qui chevauchaient la première tranche
insérée, mais les itérations suivantes travaillaient sur des ranges désynchronisées,
causant une corruption progressive des données d'emphasis et des artefacts visuels.

**Correctif** (même changement que Bug 1) : Ne pas appeler `emphapplylocal` dans
`textinsert`/`textdelete`, seulement `emphshift` (qui décale les positions sans
refaire la recherche). Laisser `textcommit` faire le refresh+apply une seule fois.

**Résultat** : Le paste n'appelle plus `emphapplylocal()` en boucle. Après la
boucle, un seul commit final rafraîchit proprement l'emphasis sans corruption.

### Changements supplémentaires

`src/cmd/acme/exec.c` (cut/paste) :
- Ligne 1008: `if(docut && t->ncache != 0)` — ne vider le cache que lors du
  vrai cut, pas lors de snarf pur.
- Ligne 1072: `if(t->ncache != 0)` — vider le cache avant paste pour éviter
  les conflits de cache/données.

`src/cmd/acme/fns.h` :
- Ajout de `void typecommit(Text*);` pour que `exec.c` puisse l'appeler.

`src/cmd/acme/text.c:1003` :
- (Bug 1 du plan original, déjà fix) : `t->ncache = 0` avant `emphrefreshlocal`
  pour prévenir la récursion infinie textcommit→textfill→typecommit.

### Résumé

| Bug | Cause | Correctif | Impact |
|---|---|---|---|
| Crash write | emphasis appliquée sur cache (tofile==FALSE) | déférer emphasis à textcommit | Write =\> no crash ✓ |
| Corruption paste | emphapplylocal en boucle | déférer emphasis à textcommit | Paste =\> clean data ✓ |

La **clé architecturale** : centraliser tout refresh/apply d'emphasis dans
`textcommit()`, appelé une seule fois par opération logique, plutôt que dans
chaque `textinsert`/`textdelete` qui peut être appelé en boucle.
