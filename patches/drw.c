diff --git a/dwm.c b/dwm.c
index 664c527..ac8e4ec 100644
--- a/dwm.c
+++ b/dwm.c
@@ -111,6 +111,7 @@ typedef struct {
 	void (*arrange)(Monitor *);
 } Layout;
 
+typedef struct Pertag Pertag;
 struct Monitor {
 	char ltsymbol[16];
 	float mfact;
@@ -130,6 +131,7 @@ struct Monitor {
 	Monitor *next;
 	Window barwin;
 	const Layout *lt[2];
+	Pertag *pertag;
 };
 
 typedef struct {
@@ -272,6 +274,15 @@ static Window root, wmcheckwin;
 /* configuration, allows nested code to access above variables */
 #include "config.h"
 
+struct Pertag {
+	unsigned int curtag, prevtag; /* current and previous tag */
+	int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
+	float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
+	unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
+	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
+	int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
+};
+
 /* compile-time check if all tags fit into an unsigned int bit array. */
 struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };
 
@@ -632,6 +643,7 @@ Monitor *
 createmon(void)
 {
 	Monitor *m;
+	unsigned int i;
 
 	m = ecalloc(1, sizeof(Monitor));
 	m->tagset[0] = m->tagset[1] = 1;
@@ -642,6 +654,20 @@ createmon(void)
 	m->lt[0] = &layouts[0];
 	m->lt[1] = &layouts[1 % LENGTH(layouts)];
 	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
+	m->pertag = ecalloc(1, sizeof(Pertag));
+	m->pertag->curtag = m->pertag->prevtag = 1;
+
+	for (i = 0; i <= LENGTH(tags); i++) {
+		m->pertag->nmasters[i] = m->nmaster;
+		m->pertag->mfacts[i] = m->mfact;
+
+		m->pertag->ltidxs[i][0] = m->lt[0];
+		m->pertag->ltidxs[i][1] = m->lt[1];
+		m->pertag->sellts[i] = m->sellt;
+
+		m->pertag->showbars[i] = m->showbar;
+	}
+
 	return m;
 }
 
@@ -967,7 +993,7 @@ grabkeys(void)
 void
 incnmaster(const Arg *arg)
 {
-	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
+	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
 	arrange(selmon);
 }
 
@@ -1502,9 +1528,9 @@ void
 setlayout(const Arg *arg)
 {
 	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
-		selmon->sellt ^= 1;
+		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
 	if (arg && arg->v)
-		selmon->lt[selmon->sellt] = (Layout *)arg->v;
+		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
 	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
 	if (selmon->sel)
 		arrange(selmon);
@@ -1523,7 +1549,7 @@ setmfact(const Arg *arg)
 	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
 	if (f < 0.05 || f > 0.95)
 		return;
-	selmon->mfact = f;
+	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
 	arrange(selmon);
 }
 
@@ -1702,7 +1728,7 @@ tile(Monitor *m)
 void
 togglebar(const Arg *arg)
 {
-	selmon->showbar = !selmon->showbar;
+	selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
 	updatebarpos(selmon);
 	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
 	arrange(selmon);
@@ -1741,9 +1767,33 @@ void
 toggleview(const Arg *arg)
 {
 	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
+	int i;
 
 	if (newtagset) {
 		selmon->tagset[selmon->seltags] = newtagset;
+
+		if (newtagset == ~0) {
+			selmon->pertag->prevtag = selmon->pertag->curtag;
+			selmon->pertag->curtag = 0;
+		}
+
+		/* test if the user did not select the same tag */
+		if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
+			selmon->pertag->prevtag = selmon->pertag->curtag;
+			for (i = 0; !(newtagset & 1 << i); i++) ;
+			selmon->pertag->curtag = i + 1;
+		}
+
+		/* apply settings for this view */
+		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
+		selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
+		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
+		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
+		selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];
+
+		if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
+			togglebar(NULL);
+
 		focus(NULL);
 		arrange(selmon);
 	}
@@ -2038,11 +2088,37 @@ updatewmhints(Client *c)
 void
 view(const Arg *arg)
 {
+	int i;
+	unsigned int tmptag;
+
 	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
 		return;
 	selmon->seltags ^= 1; /* toggle sel tagset */
-	if (arg->ui & TAGMASK)
+	if (arg->ui & TAGMASK) {
 		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
+		selmon->pertag->prevtag = selmon->pertag->curtag;
+
+		if (arg->ui == ~0)
+			selmon->pertag->curtag = 0;
+		else {
+			for (i = 0; !(arg->ui & 1 << i); i++) ;
+			selmon->pertag->curtag = i + 1;
+		}
+	} else {
+		tmptag = selmon->pertag->prevtag;
+		selmon->pertag->prevtag = selmon->pertag->curtag;
+		selmon->pertag->curtag = tmptag;
+	}
+
+	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
+	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
+	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
+	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
+	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];
+
+	if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
+		togglebar(NULL);
+
 	focus(NULL);
 	arrange(selmon);
 }
