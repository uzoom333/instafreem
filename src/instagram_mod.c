// Instagram 452.0.1 — esconde Reels, Stories, curtidas e as abas
// fora de feed / pesquisa / perfil. Sem varrer a lista de classes
// e sem trocar layoutSubviews: isso derrubava o app na splash.
// Compila no Windows com zig, sem o SDK da Apple.

typedef unsigned long long size_t;

#define NULL ((void *)0)
#define nil NULL
#define RTLD_LAZY 0x1
#define LOG_NOTICE 5

int strcmp(const char *, const char *);
char *strstr(const char *, const char *);
void syslog(int, const char *, ...);
void *dlopen(const char *, int);
void *dlsym(void *, const char *);

typedef struct objc_object IFObject;
typedef IFObject *id;
typedef struct objc_selector *SEL;
typedef struct objc_class *Class;
typedef struct objc_method *Method;
typedef void *IMP;
typedef signed char IFBool;

extern id objc_msgSend(id, SEL, ...);
extern Class objc_getClass(const char *);
extern SEL sel_registerName(const char *);
extern Method class_getInstanceMethod(Class, SEL);
extern Method class_getClassMethod(Class, SEL);
extern IMP method_getImplementation(Method);
extern IMP method_setImplementation(Method, IMP);
extern const char *object_getClassName(id);
extern id objc_retain(id);
extern void objc_release(id);
extern Class class_getSuperclass(Class);
extern Class object_getClass(id);
extern void *objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void *);

static SEL s_isKind;
static SEL s_count;
static SEL s_objectAt;
static SEL s_addObject;
static SEL s_array;
static SEL s_copy;
static SEL s_utf8;
static SEL s_responds;
static SEL s_tabString;
static SEL s_hidden;
static SEL s_alpha;
static SEL s_userInt;
static SEL s_tag;
static SEL s_setTag;
static SEL s_subviews;
static SEL s_view;
static SEL s_addSubview;
static SEL s_bg;
static SEL s_black;
static SEL s_white;
static SEL s_setText;
static SEL s_setColor;
static SEL s_setAlign;
static SEL s_setLines;
static SEL s_setFont;
static SEL s_font;

static Class c_intent;
static Class c_array;
static Class c_mutArray;
static Class c_string;
static Class c_view;
static Class c_vc;
static Class c_label;
static Class c_color;

static id g_accessGroup;
static id g_fakeRoot;
static id g_splashImage;
static int g_ready;

static const char *IFUTF8(id str) {
    if (!str || !s_utf8) return NULL;
    return ((const char *(*)(id, SEL))objc_msgSend)(str, s_utf8);
}

static int IFBannedName(const char *s) {
    if (!s || !s[0]) return 0;
    return strcmp(s, "CLIPS") == 0 || strcmp(s, "REELS") == 0;
}

static int IFBannedTab(id surface) {
    if (!surface || !c_intent || !c_string) return 0;
    IFBool kind = ((IFBool (*)(id, SEL, Class))objc_msgSend)(surface, s_isKind, c_intent);
    if (!kind) return 0;
    IFBool ok = ((IFBool (*)(id, SEL, SEL))objc_msgSend)(surface, s_responds, s_tabString);
    if (!ok) return 0;
    id tab = ((id (*)(id, SEL))objc_msgSend)(surface, s_tabString);
    if (!tab) return 0;
    if (!((IFBool (*)(id, SEL, Class))objc_msgSend)(tab, s_isKind, c_string)) return 0;
    return IFBannedName(IFUTF8(tab));
}

static unsigned long long IFCount(id obj) {
    if (!obj) return 0;
    return ((unsigned long long (*)(id, SEL))objc_msgSend)(obj, s_count);
}

static int IFHideViewName(const char *name) {
    if (!name || strstr(name, "LaunchScreen") || strstr(name, "TabBar")) return 0;
    if (strstr(name, "InstantReply")) return 0;
    if (strstr(name, "QuickSnap") || strstr(name, "Instants")) return 1;
    if (strstr(name, "Direct")) return 0;
    if (strstr(name, "Sponsored") || strstr(name, "FeedAds") || strstr(name, "FeedItemAd")
        || strstr(name, "FeedMultiAd") || strstr(name, "SundialAd") || strstr(name, "StoryAd")
        || strstr(name, "CommentAd") || strstr(name, "DelaySkipAd") || strstr(name, "LeadGenAd")
        || strstr(name, "PartnershipAd") || strstr(name, "MuseAd")) return 1;
    return strstr(name, "StoryTray")
        || strstr(name, "StoryRing")
        || strstr(name, "StoryViewer")
        || strstr(name, "StoryHighlight")
        || strstr(name, "HighlightReel")
        || strstr(name, "ProfileStory")
        || strstr(name, "DirectVisualMessage")
        || strstr(name, "InFeedStories");
}

static void IFCollapseTray(id view) {
    SEL col = sel_registerName("setStoryTrayCollapsed:");
    SEL next = sel_registerName("nextResponder");
    id cur = view;
    for (int i = 0; i < 12 && cur; i++) {
        if (s_responds && ((IFBool (*)(id, SEL, SEL))objc_msgSend)(cur, s_responds, col)) {
            ((void (*)(id, SEL, IFBool))objc_msgSend)(cur, col, 1);
            return;
        }
        if (!s_responds || !((IFBool (*)(id, SEL, SEL))objc_msgSend)(cur, s_responds, next)) return;
        id step = ((id (*)(id, SEL))objc_msgSend)(cur, next);
        if (step == cur) return;
        cur = step;
    }
}

static id IFStr(const char *text);
static int IFShouldBlankCount(id view, id text);

static void (*orig_didMove)(id, SEL);

static void IFBlankVisibleCount(id self) {
    const char *name = object_getClassName(self);
    if (!name || !strstr(name, "Label")) return;
    if (!s_responds || !((IFBool (*)(id, SEL, SEL))objc_msgSend)(self, s_responds, s_setText)) return;
    SEL textSel = sel_registerName("text");
    if (!((IFBool (*)(id, SEL, SEL))objc_msgSend)(self, s_responds, textSel)) return;
    id text = ((id (*)(id, SEL))objc_msgSend)(self, textSel);
    if (text && IFShouldBlankCount(self, text)) {
        ((void (*)(id, SEL, id))objc_msgSend)(self, s_setText, IFStr(""));
    }
}

static void IFDidMove(id self, SEL _cmd) {
    if (orig_didMove) orig_didMove(self, _cmd);
    if (!g_ready) return;
    const char *name = object_getClassName(self);
    IFBlankVisibleCount(self);
    if (!IFHideViewName(name)) return;
    static __thread int guard;
    if (guard) return;
    guard = 1;
    ((void (*)(id, SEL, IFBool))objc_msgSend)(self, s_hidden, 1);
    ((void (*)(id, SEL, double))objc_msgSend)(self, s_alpha, 0.0);
    ((void (*)(id, SEL, IFBool))objc_msgSend)(self, s_userInt, 0);
    if (name && strstr(name, "StoryTray")) IFCollapseTray(self);
    guard = 0;
}

static int IFSplashName(const char *name) {
    return name && strstr(name, "IGLaunchScreenAnimationViewController");
}

static id IFNew(Class cls) {
    id obj = ((id (*)(id, SEL))objc_msgSend)((id)cls, sel_registerName("alloc"));
    return ((id (*)(id, SEL))objc_msgSend)(obj, sel_registerName("init"));
}

static void IFPin(id child, id parent, long long attribute) {
    Class cls = objc_getClass("NSLayoutConstraint");
    if (!cls || !child || !parent) return;
    id constraint = ((id (*)(id, SEL, id, long long, long long, id, long long, double, double))objc_msgSend)(
        (id)cls,
        sel_registerName("constraintWithItem:attribute:relatedBy:toItem:attribute:multiplier:constant:"),
        child, attribute, (long long)0, parent, attribute, 1.0, 0.0);
    if (!constraint) return;
    ((void (*)(id, SEL, id))objc_msgSend)(parent, sel_registerName("addConstraint:"), constraint);
}

static void IFCoverSplashReal(id vc) {
    if (!c_view || !c_label || !c_color || !c_string) return;
    id view = ((id (*)(id, SEL))objc_msgSend)(vc, s_view);
    if (!view) return;
    id subs = ((id (*)(id, SEL))objc_msgSend)(view, s_subviews);
    unsigned long long count = IFCount(subs);
    for (unsigned long long i = 0; i < count; i++) {
        id sub = ((id (*)(id, SEL, unsigned long long))objc_msgSend)(subs, s_objectAt, i);
        if (((long long (*)(id, SEL))objc_msgSend)(sub, s_tag) == 452452) return;
    }
    id overlay = IFNew(c_view);
    id label = IFNew(c_label);
    if (!overlay || !label) return;
    ((void (*)(id, SEL, IFBool))objc_msgSend)(overlay, sel_registerName("setTranslatesAutoresizingMaskIntoConstraints:"), 0);
    ((void (*)(id, SEL, IFBool))objc_msgSend)(label, sel_registerName("setTranslatesAutoresizingMaskIntoConstraints:"), 0);
    id black = ((id (*)(id, SEL))objc_msgSend)((id)c_color, s_black);
    id white = ((id (*)(id, SEL))objc_msgSend)((id)c_color, s_white);
    id font = ((id (*)(id, SEL, double))objc_msgSend)((id)objc_getClass("UIFont"), s_font, 22.0);
    id text = ((id (*)(id, SEL, const char *))objc_msgSend)((id)c_string, sel_registerName("stringWithUTF8String:"), "modded by Aeromorto");
    ((void (*)(id, SEL, long long))objc_msgSend)(overlay, s_setTag, 452452);
    ((void (*)(id, SEL, id))objc_msgSend)(overlay, s_bg, black);
    ((void (*)(id, SEL, id))objc_msgSend)(label, s_setText, text);
    ((void (*)(id, SEL, id))objc_msgSend)(label, s_setColor, white);
    ((void (*)(id, SEL, long long))objc_msgSend)(label, s_setAlign, 1);
    ((void (*)(id, SEL, long long))objc_msgSend)(label, s_setLines, 0);
    if (font) ((void (*)(id, SEL, id))objc_msgSend)(label, s_setFont, font);
    ((void (*)(id, SEL, id))objc_msgSend)(overlay, s_addSubview, label);
    ((void (*)(id, SEL, id))objc_msgSend)(view, s_addSubview, overlay);
    IFPin(overlay, view, 3);
    IFPin(overlay, view, 4);
    IFPin(overlay, view, 5);
    IFPin(overlay, view, 6);
    IFPin(label, overlay, 3);
    IFPin(label, overlay, 4);
    IFPin(label, overlay, 5);
    IFPin(label, overlay, 6);
}

static void (*orig_willAppear)(id, SEL, IFBool);

static void IFLockScrollView(id scroll) {
    if (!scroll) return;
    ((void (*)(id, SEL, IFBool))objc_msgSend)(scroll, sel_registerName("setScrollEnabled:"), 0);
    ((void (*)(id, SEL, IFBool))objc_msgSend)(scroll, sel_registerName("setAlwaysBounceVertical:"), 0);
}

static void IFLockTree(id view, int depth) {
    if (!view || depth > 7 || !s_subviews) return;
    const char *name = object_getClassName(view);
    if (name && (strstr(name, "ScrollView") || strstr(name, "CollectionView"))) IFLockScrollView(view);
    id subs = ((id (*)(id, SEL))objc_msgSend)(view, s_subviews);
    unsigned long long count = IFCount(subs);
    for (unsigned long long i = 0; i < count; i++) {
        IFLockTree(((id (*)(id, SEL, unsigned long long))objc_msgSend)(subs, s_objectAt, i), depth + 1);
    }
}

static int IFChainHasDirect(id vc) {
    SEL pres = sel_registerName("presentingViewController");
    id cur = vc;
    for (int i = 0; i < 6 && cur; i++) {
        const char *name = object_getClassName(cur);
        if (name && strstr(name, "Direct")) return 1;
        if (!s_responds || !((IFBool (*)(id, SEL, SEL))objc_msgSend)(cur, s_responds, pres)) return 0;
        id next = ((id (*)(id, SEL))objc_msgSend)(cur, pres);
        if (!next || next == cur) return 0;
        cur = next;
    }
    return 0;
}

static int IFDirectReel(id vc) {
    const char *name = object_getClassName(vc);
    if (!name || strstr(name, "HomeSundial")) return 0;
    if (!(strstr(name, "Sundial") || strstr(name, "ClipsViewer"))) return 0;
    if (strstr(name, "Direct")) return 1;
    return IFChainHasDirect(vc);
}

static void IFWillAppear(id self, SEL _cmd, IFBool animated) {
    if (orig_willAppear) orig_willAppear(self, _cmd, animated);
    if (!g_ready) return;
    if (IFDirectReel(self)) {
        id view = ((id (*)(id, SEL))objc_msgSend)(self, s_view);
        IFLockTree(view, 0);
    }
    if (!IFSplashName(object_getClassName(self))) return;
    static __thread int guard;
    if (guard) return;
    guard = 1;
    IFCoverSplashReal(self);
    guard = 0;
}

static int IFSplashImageName(const char *name) {
    if (!name) return 0;
    return strcmp(name, "logo-splash") == 0
        || strcmp(name, "Metal") == 0
        || strcmp(name, "fb-branding") == 0
        || strcmp(name, "aeromorto-m") == 0
        || strcmp(name, "aerom-logo1") == 0
        || strstr(name, "logo-splash") != NULL
        || strstr(name, "fb-branding") != NULL;
}

static __thread int g_imageGuard;

static id (*orig_imageNamed)(id, SEL, id);
static id IFImageNamed(id self, SEL _cmd, id name) {
    if (g_imageGuard) return orig_imageNamed ? orig_imageNamed(self, _cmd, name) : nil;
    if (g_splashImage && IFSplashImageName(IFUTF8(name))) return g_splashImage;
    g_imageGuard = 1;
    id image = orig_imageNamed ? orig_imageNamed(self, _cmd, name) : nil;
    g_imageGuard = 0;
    return image;
}

static id (*orig_imageNamedBundle)(id, SEL, id, id, id);
static id IFImageNamedBundle(id self, SEL _cmd, id name, id bundle, id trait) {
    if (g_imageGuard) return orig_imageNamedBundle ? orig_imageNamedBundle(self, _cmd, name, bundle, trait) : nil;
    if (g_splashImage && IFSplashImageName(IFUTF8(name))) return g_splashImage;
    g_imageGuard = 1;
    id image = orig_imageNamedBundle ? orig_imageNamedBundle(self, _cmd, name, bundle, trait) : nil;
    g_imageGuard = 0;
    return image;
}

static int IFSwizzle(Class cls, const char *selName, void *replacement, void **origOut) {
    if (!cls) return 0;
    SEL sel = sel_registerName(selName);
    Method method = class_getInstanceMethod(cls, sel);
    if (!method) return 0;
    if (origOut) *origOut = (void *)method_getImplementation(method);
    method_setImplementation(method, (IMP)replacement);
    return 1;
}

static int IFSwizzleClass(Class cls, const char *selName, void *replacement, void **origOut) {
    if (!cls) return 0;
    SEL sel = sel_registerName(selName);
    Method method = class_getClassMethod(cls, sel);
    if (!method) return 0;
    if (origOut) *origOut = (void *)method_getImplementation(method);
    method_setImplementation(method, (IMP)replacement);
    return 1;
}

static id IFStr(const char *text) {
    if (!c_string || !text) return nil;
    return ((id (*)(id, SEL, const char *))objc_msgSend)((id)c_string, sel_registerName("stringWithUTF8String:"), text);
}

static id IFAccessGroupIMP(id self, SEL _cmd) {
    (void)self;
    (void)_cmd;
    return g_accessGroup;
}

static void IFHookAccessGroup(const char *className) {
    Class cls = objc_getClass(className);
    if (!cls || !g_accessGroup) return;
    SEL sel = sel_registerName("accessGroup");
    Method method = class_getInstanceMethod(cls, sel);
    if (!method) return;
    method_setImplementation(method, (IMP)IFAccessGroupIMP);
}

static void IFDiscoverAccessGroup(void) {
    void *security = dlopen("/System/Library/Frameworks/Security.framework/Security", RTLD_LAZY);
    if (!security) return;
    typedef int (*SecFn)(void *, void **);
    SecFn copyMatching = (SecFn)dlsym(security, "SecItemCopyMatching");
    SecFn itemAdd = (SecFn)dlsym(security, "SecItemAdd");
    id kClass = IFStr("class");
    id kGeneric = IFStr("genp");
    id kAccount = IFStr("acct");
    id kService = IFStr("svce");
    id kReturn = IFStr("r_Attributes");
    id kGroup = IFStr("agrp");
    if (!copyMatching || !itemAdd || !kClass || !kGeneric || !kAccount || !kService || !kReturn || !kGroup) return;

    Class strCls = c_string;
    Class numCls = objc_getClass("NSNumber");
    Class arrCls = c_array;
    Class dictCls = objc_getClass("NSDictionary");
    if (!strCls || !numCls || !arrCls || !dictCls) return;

    id account = ((id (*)(id, SEL, const char *))objc_msgSend)((id)strCls, sel_registerName("stringWithUTF8String:"), "aeromorto.seed");
    id service = ((id (*)(id, SEL, const char *))objc_msgSend)((id)strCls, sel_registerName("stringWithUTF8String:"), "aeromorto");
    id yes = ((id (*)(id, SEL, IFBool))objc_msgSend)((id)numCls, sel_registerName("numberWithBool:"), 1);
    id objects[4];
    id keys[4];
    objects[0] = kGeneric;
    keys[0] = kClass;
    objects[1] = account;
    keys[1] = (id)kAccount;
    objects[2] = service;
    keys[2] = (id)kService;
    objects[3] = yes;
    keys[3] = (id)kReturn;
    id objArr = ((id (*)(id, SEL, id *, unsigned long long))objc_msgSend)((id)arrCls, sel_registerName("arrayWithObjects:count:"), objects, 4);
    id keyArr = ((id (*)(id, SEL, id *, unsigned long long))objc_msgSend)((id)arrCls, sel_registerName("arrayWithObjects:count:"), keys, 4);
    id query = ((id (*)(id, SEL, id, id))objc_msgSend)((id)dictCls, sel_registerName("dictionaryWithObjects:forKeys:"), objArr, keyArr);
    if (!query) return;

    void *result = NULL;
    int status = copyMatching(query, &result);
    if (status == -25300) status = itemAdd(query, &result);
    if (status != 0 || !result) return;
    id group = ((id (*)(id, SEL, id))objc_msgSend)((id)result, sel_registerName("objectForKey:"), kGroup);
    if (group && c_string && ((IFBool (*)(id, SEL, Class))objc_msgSend)(group, s_isKind, c_string)) {
        g_accessGroup = objc_retain(group);
    }
    objc_release((id)result);
}

static void IFMakeDir(id fm, id url) {
    if (!fm || !url) return;
    ((IFBool (*)(id, SEL, id, IFBool, id, void *))objc_msgSend)(
        fm,
        sel_registerName("createDirectoryAtURL:withIntermediateDirectories:attributes:error:"),
        url, 1, nil, NULL);
}

static id IFChild(id url, const char *name) {
    if (!url || !c_string) return nil;
    id part = ((id (*)(id, SEL, const char *))objc_msgSend)((id)c_string, sel_registerName("stringWithUTF8String:"), name);
    return ((id (*)(id, SEL, id))objc_msgSend)(url, sel_registerName("URLByAppendingPathComponent:"), part);
}

static id (*orig_containerURL)(id, SEL, id);

static id IFContainerURL(id self, SEL _cmd, id identifier) {
    static __thread int guard;
    if (guard || !g_fakeRoot || !identifier || !c_string || !s_isKind) {
        return orig_containerURL ? orig_containerURL(self, _cmd, identifier) : nil;
    }
    if (!((IFBool (*)(id, SEL, Class))objc_msgSend)(identifier, s_isKind, c_string)) {
        return orig_containerURL ? orig_containerURL(self, _cmd, identifier) : nil;
    }
    unsigned long long length = ((unsigned long long (*)(id, SEL))objc_msgSend)(identifier, sel_registerName("length"));
    if (length == 0) {
        return orig_containerURL ? orig_containerURL(self, _cmd, identifier) : nil;
    }
    guard = 1;
    id folder = ((id (*)(id, SEL, id))objc_msgSend)(g_fakeRoot, sel_registerName("URLByAppendingPathComponent:"), identifier);
    id fm = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSFileManager"), sel_registerName("defaultManager"));
    IFMakeDir(fm, folder);
    IFMakeDir(fm, IFChild(folder, "Library"));
    IFMakeDir(fm, IFChild(folder, "Library/Caches"));
    guard = 0;
    return folder;
}

static void IFPrepareGroupURL(void) {
    void *foundation = dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_LAZY);
    id (*home)(void) = foundation ? (id (*)(void))dlsym(foundation, "NSHomeDirectory") : NULL;
    if (!home || !c_string) return;
    id homeStr = home();
    if (!homeStr) return;
    id docs = ((id (*)(id, SEL, id))objc_msgSend)(homeStr, sel_registerName("stringByAppendingPathComponent:"), IFStr("Documents/FakeGroupContainers"));
    g_fakeRoot = ((id (*)(id, SEL, id, IFBool))objc_msgSend)((id)objc_getClass("NSURL"), sel_registerName("fileURLWithPath:isDirectory:"), docs, 1);
    if (g_fakeRoot) g_fakeRoot = objc_retain(g_fakeRoot);
    id fm = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSFileManager"), sel_registerName("defaultManager"));
    IFMakeDir(fm, g_fakeRoot);
}

static void IFLoadSplashImage(void) {
    id bundle = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSBundle"), sel_registerName("mainBundle"));
    if (!bundle) return;
    id path = ((id (*)(id, SEL, id, id))objc_msgSend)(
        bundle, sel_registerName("pathForResource:ofType:"), IFStr("AeromortoSplash"), IFStr("png"));
    if (!path) return;
    id image = ((id (*)(id, SEL, id))objc_msgSend)((id)objc_getClass("UIImage"), sel_registerName("imageWithContentsOfFile:"), path);
    if (image) g_splashImage = objc_retain(image);
}

static int IFSwizzleOwn(Class cls, const char *selName, void *replacement, void **origOut) {
    if (!cls) return 0;
    SEL sel = sel_registerName(selName);
    Method mine = class_getInstanceMethod(cls, sel);
    Class sup = class_getSuperclass(cls);
    Method parent = sup ? class_getInstanceMethod(sup, sel) : NULL;
    if (!mine || mine == parent) return 0;
    if (origOut) *origOut = (void *)method_getImplementation(mine);
    method_setImplementation(mine, (IMP)replacement);
    return 1;
}

static void IFHideSelf(id self) {
    if (!self || !s_hidden) return;
    ((void (*)(id, SEL, IFBool))objc_msgSend)(self, s_hidden, 1);
    ((void (*)(id, SEL, double))objc_msgSend)(self, s_alpha, 0.0);
    ((void (*)(id, SEL, IFBool))objc_msgSend)(self, s_userInt, 0);
}

static int IFLabelHasReels(id text) {
    const char *s = IFUTF8(text);
    if (!s) return 0;
    return strstr(s, "Reel") || strstr(s, "reel") || strstr(s, "Clip") || strstr(s, "clip");
}

static int IFIsReelsButton(id controller, id button) {
    if (!button) return 0;
    SEL surfSel = sel_registerName("_surfaceForTabBarButton:");
    if (controller && s_responds && ((IFBool (*)(id, SEL, SEL))objc_msgSend)(controller, s_responds, surfSel)) {
        id surface = ((id (*)(id, SEL, id))objc_msgSend)(controller, surfSel, button);
        if (IFBannedTab(surface)) return 1;
    }
    if (IFLabelHasReels(((id (*)(id, SEL))objc_msgSend)(button, sel_registerName("accessibilityLabel")))) return 1;
    if (IFLabelHasReels(((id (*)(id, SEL))objc_msgSend)(button, sel_registerName("accessibilityIdentifier")))) return 1;
    return 0;
}

static void (*orig_tap)(id, SEL, id);
static void IFTap(id self, SEL _cmd, id button) {
    if (IFIsReelsButton(self, button)) {
        IFHideSelf(button);
        return;
    }
    if (orig_tap) orig_tap(self, _cmd, button);
}

static void (*orig_tapUnder)(id, SEL, id);
static void IFTapUnder(id self, SEL _cmd, id button) {
    if (IFIsReelsButton(self, button)) {
        IFHideSelf(button);
        return;
    }
    if (orig_tapUnder) orig_tapUnder(self, _cmd, button);
}

typedef struct { double x, y, w, h; } IFRect;

static IFRect IFBounds(id view) {
    return ((IFRect (*)(id, SEL))objc_msgSend)(view, sel_registerName("bounds"));
}

static void IFSetFrame(id view, IFRect frame) {
    ((void (*)(id, SEL, IFRect))objc_msgSend)(view, sel_registerName("setFrame:"), frame);
}

static IFRect IFFrame(id view) {
    return ((IFRect (*)(id, SEL))objc_msgSend)(view, sel_registerName("frame"));
}

static int IFTabButton(const char *name) {
    return name && (strstr(name, "TabBarButton") || strstr(name, "TabItem"));
}

static void IFReflow(id bar) {
    if (!bar || !s_subviews || !s_objectAt) return;
    id subs = ((id (*)(id, SEL))objc_msgSend)(bar, s_subviews);
    unsigned long long count = IFCount(subs);
    id keep[8];
    int n = 0;
    int removed = 0;
    for (unsigned long long i = 0; i < count; i++) {
        id sub = ((id (*)(id, SEL, unsigned long long))objc_msgSend)(subs, s_objectAt, i);
        const char *name = object_getClassName(sub);
        if (!IFTabButton(name)) continue;
        long long tag = ((long long (*)(id, SEL))objc_msgSend)(sub, s_tag);
        if (tag == 452453 || IFIsReelsButton(nil, sub)) {
            IFHideSelf(sub);
            ((void (*)(id, SEL, long long))objc_msgSend)(sub, s_setTag, (long long)452453);
            removed = 1;
            continue;
        }
        if (n < 8) keep[n++] = sub;
    }
    if (!removed || n < 2) return;
    IFRect bounds = IFBounds(bar);
    if (bounds.w < 80.0 || bounds.w > 4000.0) return;
    double width = bounds.w / (double)n;
    for (int i = 0; i < n; i++) {
        IFRect old = IFFrame(keep[i]);
        IFRect frame;
        frame.x = width * (double)i;
        frame.y = old.y;
        frame.w = width;
        frame.h = old.h;
        IFSetFrame(keep[i], frame);
    }
}

static void (*orig_btnLayout)(id, SEL);
static void IFBtnLayout(id self, SEL _cmd) {
    if (orig_btnLayout) orig_btnLayout(self, _cmd);
    id ctrl = nil;
    id responder = self;
    SEL nextSel = sel_registerName("nextResponder");
    SEL surfSel = sel_registerName("_surfaceForTabBarButton:");
    for (int i = 0; i < 14 && responder; i++) {
        responder = ((id (*)(id, SEL))objc_msgSend)(responder, nextSel);
        if (responder && s_responds && ((IFBool (*)(id, SEL, SEL))objc_msgSend)(responder, s_responds, surfSel)) {
            ctrl = responder;
            break;
        }
    }
    if (IFIsReelsButton(ctrl, self)) {
        IFHideSelf(self);
        ((void (*)(id, SEL, long long))objc_msgSend)(self, s_setTag, (long long)452453);
    }
    id parent = ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("superview"));
    if (parent) IFReflow(parent);
}

static int IFInstantsViewer(id vc) {
    const char *name = vc ? object_getClassName(vc) : NULL;
    if (!name || strstr(name, "InstantReply")) return 0;
    return strstr(name, "QuickSnap") || strstr(name, "Instants") || strstr(name, "StoryInstants");
}

static int IFBlockedViewer(id vc) {
    if (!vc) return 0;
    const char *name = object_getClassName(vc);
    if (name && strstr(name, "HomeSundial")) return 0;
    if (name && (strstr(name, "SundialFeedViewController") || strstr(name, "ClipsViewer"))) return 1;
    SEL topSel = sel_registerName("topViewController");
    if (s_responds && ((IFBool (*)(id, SEL, SEL))objc_msgSend)(vc, s_responds, topSel)) {
        id top = ((id (*)(id, SEL))objc_msgSend)(vc, topSel);
        if (top && top != vc) return IFBlockedViewer(top);
    }
    return 0;
}

static int IFInsideDirectReel(id start) {
    SEL next = sel_registerName("nextResponder");
    SEL pres = sel_registerName("presentingViewController");
    id cur = start;
    for (int i = 0; i < 18 && cur; i++) {
        const char *name = object_getClassName(cur);
        if (name && !strstr(name, "HomeSundial") && (strstr(name, "Sundial") || strstr(name, "ClipsViewer"))) {
            if (strstr(name, "Direct")) return 1;
            if (s_responds && ((IFBool (*)(id, SEL, SEL))objc_msgSend)(cur, s_responds, pres)) {
                id parent = ((id (*)(id, SEL))objc_msgSend)(cur, pres);
                const char *parentName = parent ? object_getClassName(parent) : NULL;
                if (parentName && strstr(parentName, "Direct")) return 1;
            }
        }
        if (!s_responds || !((IFBool (*)(id, SEL, SEL))objc_msgSend)(cur, s_responds, next)) return 0;
        id step = ((id (*)(id, SEL))objc_msgSend)(cur, next);
        if (!step || step == cur) return 0;
        cur = step;
    }
    return 0;
}

static void (*orig_setScroll)(id, SEL, IFBool);
static void IFSetScroll(id self, SEL _cmd, IFBool enabled) {
    if (enabled && IFInsideDirectReel(self)) enabled = 0;
    if (orig_setScroll) orig_setScroll(self, _cmd, enabled);
}

typedef struct { double w, h; } IFSize;

static IFSize IFZeroSize(id self, SEL _cmd, long long index) {
    (void)self;
    (void)_cmd;
    (void)index;
    IFSize size;
    size.w = 0;
    size.h = 0;
    return size;
}

static int IFZeroSection(const char *name) {
    Class cls = objc_getClass(name);
    if (!cls) return 0;
    return IFSwizzleOwn(cls, "sizeForItemAtIndex:", (void *)IFZeroSize, NULL);
}

static int IFResponds(id obj, const char *selName) {
    if (!obj || !s_responds) return 0;
    return ((IFBool (*)(id, SEL, SEL))objc_msgSend)(obj, s_responds, sel_registerName(selName));
}

static IFBool IFBoolSel(id obj, const char *selName) {
    if (!IFResponds(obj, selName)) return 0;
    return ((IFBool (*)(id, SEL))objc_msgSend)(obj, sel_registerName(selName));
}

static __thread int g_sizeGuard;

static int IFSectionDrop(id self) {
    if (!self || g_sizeGuard) return 0;
    g_sizeGuard = 1;
    int drop = IFBoolSel(self, "_isSponsoredPost")
        || IFBoolSel(self, "alv2_isSponsored")
        || IFBoolSel(self, "_isCurrentPostItemVideo");
    if (!drop && IFResponds(self, "media")) {
        id media = ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("media"));
        if (media) {
            drop = IFBoolSel(media, "isSponsored")
                || IFBoolSel(media, "isClipsMedia")
                || IFBoolSel(media, "isFeedVideo")
                || IFBoolSel(media, "_isSponsoredPost");
        }
    }
    g_sizeGuard = 0;
    return drop;
}

static IFSize (*orig_sizeItem)(id, SEL, long long);
static IFSize (*orig_calcSize)(id, SEL, long long);

static IFSize IFSizeItem(id self, SEL _cmd, long long index) {
    if (g_ready && IFSectionDrop(self)) {
        IFSize zero;
        zero.w = 0;
        zero.h = 0;
        return zero;
    }
    if (orig_sizeItem) return orig_sizeItem(self, _cmd, index);
    IFSize zero;
    zero.w = 0;
    zero.h = 0;
    return zero;
}

static IFSize IFCalcSize(id self, SEL _cmd, long long index) {
    if (g_ready && IFSectionDrop(self)) {
        IFSize zero;
        zero.w = 0;
        zero.h = 0;
        return zero;
    }
    if (orig_calcSize) return orig_calcSize(self, _cmd, index);
    IFSize zero;
    zero.w = 0;
    zero.h = 0;
    return zero;
}

static void (*orig_present)(id, SEL, id, IFBool, void *);
static void IFPresent(id self, SEL _cmd, id vc, IFBool animated, void *done) {
    if (IFInstantsViewer(vc)) return;
    int blocked = IFBlockedViewer(vc);
    int direct = IFChainHasDirect(self) || IFChainHasDirect(vc) || (object_getClassName(vc) && strstr(object_getClassName(vc), "Direct"));
    if (blocked && !direct) return;
    if (orig_present) orig_present(self, _cmd, vc, animated, done);
}

static void (*orig_push)(id, SEL, id, IFBool);
static void IFPush(id self, SEL _cmd, id vc, IFBool animated) {
    if (IFInstantsViewer(vc)) return;
    int blocked = IFBlockedViewer(vc);
    int direct = IFChainHasDirect(self) || IFChainHasDirect(vc) || (object_getClassName(vc) && strstr(object_getClassName(vc), "Direct"));
    if (blocked && !direct) return;
    if (orig_push) orig_push(self, _cmd, vc, animated);
}

static int IFHasDigit(const char *s) {
    for (; s && *s; s++) if (*s >= '0' && *s <= '9') return 1;
    return 0;
}

static size_t IFLen(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static int IFEndsWith(const char *s, const char *suffix) {
    size_t n = IFLen(s);
    size_t m = IFLen(suffix);
    if (n < m) return 0;
    return strcmp(s + (n - m), suffix) == 0;
}

static int IFLikePhrase(const char *raw) {
    if (!raw || !raw[0]) return 0;
    char buf[96];
    size_t n = 0;
    for (; raw[n] && n + 1 < sizeof(buf); n++) {
        char c = raw[n];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        buf[n] = c;
    }
    if (raw[n]) return 0;
    buf[n] = 0;
    if (n == 0 || n > 80) return 0;
    // setText: chega antes de o label entrar na tela, sem superview para dizer se é DM.
    // Por isso a frase precisa ser a linha inteira de contagem, e não uma palavra
    // solta: "isso é publicidade" numa DM não pode sumir.
    if (strstr(buf, "liked by ") == buf || strstr(buf, "curtido por ") == buf) return 1;
    if (strcmp(buf, "sponsored") == 0 || strcmp(buf, "patrocinado") == 0 || strcmp(buf, "publicidade") == 0) return 1;
    const char *words[] = {
        " curtidas", " curtida", " likes", " like",
        " comments", " comment", " comentarios", " comentario",
        " coment\u00e1rios", " coment\u00e1rio",
        " reposts", " repost", " republicacoes", " republica\u00e7\u00f5es",
        NULL
    };
    for (int i = 0; words[i]; i++) {
        if (!IFEndsWith(buf, words[i])) continue;
        size_t m = IFLen(words[i]);
        int ok = 1;
        for (size_t k = 0; k < n - m; k++) {
            char c = buf[k];
            if (!((c >= '0' && c <= '9') || c == '.' || c == ',' || c == ' ')) ok = 0;
        }
        if (ok && IFHasDigit(buf)) return 1;
    }
    return 0;
}

static int IFInCountView(id view) {
    id current = view;
    SEL superSel = sel_registerName("superview");
    for (int i = 0; i < 10 && current; i++) {
        const char *name = object_getClassName(current);
        if (name && !(strstr(name, "Composer") || strstr(name, "TextView") || strstr(name, "Editor") || strstr(name, "Search"))) {
            if (strstr(name, "UFI") || strstr(name, "LikeCount") || strstr(name, "CommentCount")
                || strstr(name, "ShareCount") || strstr(name, "Repost") || strstr(name, "SocialContext")
                || strstr(name, "FollowedBy") || strstr(name, "PreviewComment")) return 1;
        }
        current = ((id (*)(id, SEL))objc_msgSend)(current, superSel);
    }
    return 0;
}

static int IFPureCount(const char *raw) {
    if (!raw) return 0;
    char buf[24];
    size_t n = 0;
    for (; raw[n] && n + 1 < sizeof(buf); n++) {
        char c = raw[n];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        buf[n] = c;
    }
    if (raw[n]) return 0;
    buf[n] = 0;
    if (n == 0 || n > 16) return 0;
    const char *mil = strstr(buf, " mil");
    size_t prefix = mil ? (size_t)(mil - buf) : n;
    int digit = 0;
    for (size_t i = 0; i < prefix; i++) {
        char c = buf[i];
        if (c >= '0' && c <= '9') { digit = 1; continue; }
        if (c == '.' || c == ',' || c == ' ' || c == 'k' || c == 'm') continue;
        return 0;
    }
    return digit;
}

static int IFShouldBlankCount(id view, id text) {
    const char *raw = IFUTF8(text);
    if (!raw || !raw[0]) return 0;
    if (IFLikePhrase(raw)) return 1;
    if (!IFInCountView(view)) return 0;
    return IFPureCount(raw) || IFHasDigit(raw);
}

static void (*orig_setText)(id, SEL, id);
static void IFSetText(id self, SEL _cmd, id text) {
    if (text && IFShouldBlankCount(self, text)) text = IFStr("");
    if (orig_setText) orig_setText(self, _cmd, text);
}

static void (*orig_setAttr)(id, SEL, id);
static void IFSetAttr(id self, SEL _cmd, id text) {
    if (text) {
        id plain = ((id (*)(id, SEL))objc_msgSend)(text, sel_registerName("string"));
        if (plain && IFShouldBlankCount(self, plain)) {
            Class cls = objc_getClass("NSAttributedString");
            id empty = cls ? ((id (*)(id, SEL))objc_msgSend)((id)cls, sel_registerName("alloc")) : nil;
            if (empty) {
                empty = ((id (*)(id, SEL, id))objc_msgSend)(empty, sel_registerName("initWithString:"), IFStr(""));
                text = empty;
            }
        }
    }
    if (orig_setAttr) orig_setAttr(self, _cmd, text);
}

static void (*orig_setTitle)(id, SEL, id, unsigned long long);
static void IFSetTitle(id self, SEL _cmd, id title, unsigned long long state) {
    if (title && IFShouldBlankCount(self, title)) title = IFStr("");
    if (orig_setTitle) orig_setTitle(self, _cmd, title, state);
}

static void (*orig_setCount)(id, SEL, id, IFBool);
static void IFSetCount(id self, SEL _cmd, id count, IFBool show) {
    (void)count;
    if (orig_setCount) orig_setCount(self, _cmd, IFStr(""), show);
}

static void IFInstall(void) {
    if (g_ready) return;
    dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_LAZY);
    dlopen("/System/Library/Frameworks/UIKit.framework/UIKit", RTLD_LAZY);

    s_isKind = sel_registerName("isKindOfClass:");
    s_count = sel_registerName("count");
    s_objectAt = sel_registerName("objectAtIndex:");
    s_addObject = sel_registerName("addObject:");
    s_array = sel_registerName("array");
    s_copy = sel_registerName("copy");
    s_utf8 = sel_registerName("UTF8String");
    s_responds = sel_registerName("respondsToSelector:");
    s_tabString = sel_registerName("tabStringFromSurfaceIntent");
    s_hidden = sel_registerName("setHidden:");
    s_alpha = sel_registerName("setAlpha:");
    s_userInt = sel_registerName("setUserInteractionEnabled:");
    s_tag = sel_registerName("tag");
    s_setTag = sel_registerName("setTag:");
    s_subviews = sel_registerName("subviews");
    s_view = sel_registerName("view");
    s_addSubview = sel_registerName("addSubview:");
    s_bg = sel_registerName("setBackgroundColor:");
    s_black = sel_registerName("blackColor");
    s_white = sel_registerName("whiteColor");
    s_setText = sel_registerName("setText:");
    s_setColor = sel_registerName("setTextColor:");
    s_setAlign = sel_registerName("setTextAlignment:");
    s_setLines = sel_registerName("setNumberOfLines:");
    s_setFont = sel_registerName("setFont:");
    s_font = sel_registerName("systemFontOfSize:");

    c_intent = objc_getClass("IGMainAppSurfaceIntent");
    c_array = objc_getClass("NSArray");
    c_mutArray = objc_getClass("NSMutableArray");
    c_string = objc_getClass("NSString");
    c_view = objc_getClass("UIView");
    c_vc = objc_getClass("UIViewController");
    c_label = objc_getClass("UILabel");
    c_color = objc_getClass("UIColor");

    Class tab = objc_getClass("IGTabBarController");
    int hTabs = IFSwizzle(tab, "didTapTabBarButton:", IFTap, (void **)&orig_tap);
    hTabs += IFSwizzle(tab, "_didTapTabBarButton:", IFTapUnder, (void **)&orig_tapUnder);
    hTabs += IFSwizzleOwn(objc_getClass("IGTabBarButton"), "layoutSubviews", IFBtnLayout, (void **)&orig_btnLayout);
    int hHide = IFSwizzle(c_view, "didMoveToWindow", IFDidMove, (void **)&orig_didMove);
    int hLikes = IFSwizzle(c_label, "setText:", IFSetText, (void **)&orig_setText);
    hLikes += IFSwizzle(c_label, "setAttributedText:", IFSetAttr, (void **)&orig_setAttr);
    hLikes += IFSwizzle(objc_getClass("UIButton"), "setTitle:forState:", IFSetTitle, (void **)&orig_setTitle);
    hLikes += IFSwizzle(objc_getClass("IGUFIButtonWithCountsView"), "setCountString:showButton:", IFSetCount, (void **)&orig_setCount);
    int hSections = IFZeroSection("IGStoryTraySectionController");
    hSections += IFZeroSection("IGStoryTrayShimmeringSectionController");
    hSections += IFZeroSection("IGStoryTrayOurStorySectionController");
    hSections += IFZeroSection("IGFeedScrollableClipsSectionController");
    hSections += IFZeroSection("IGFeedScrollableClipCellSectionController");
    hSections += IFZeroSection("_TtC26IGFeedScrollableClipsSwift38IGFeedScrollableClipsSectionController");
    hSections += IFZeroSection("_TtC26IGFeedScrollableClipsSwift41IGFeedScrollableClipCellSectionController");
    hSections += IFZeroSection("_TtC26IGFeedScrollableClipsSwift51IGFeedScrollableClipsSectionController");
    hSections += IFSwizzle(objc_getClass("IGFeedSectionControllerOld"), "sizeForItemAtIndex:", IFSizeItem, (void **)&orig_sizeItem);
    hSections += IFSwizzle(objc_getClass("IGFeedSectionControllerOld"), "_calculateSizeForItemAtIndex:", IFCalcSize, (void **)&orig_calcSize);
    hSections += IFZeroSection("IGDiscoveryGridAdSectionController");
    hSections += IFZeroSection("IGContextualAdPivotCarouselSectionController");
    hSections += IFZeroSection("IGContextualAdsPivotBrowseCardSectionController");
    hSections += IFZeroSection("IGIntentAwareAdsPivotBrowseCardSectionController");
    hSections += IFZeroSection("IGIntentAwareAdsPivotCarouselSectionController");
    hSections += IFZeroSection("IGSundialAd4AdSectionController");
    hSections += IFZeroSection("IGSundialAd4AdCreatorsSectionController");
    hSections += IFZeroSection("IGVideoViewCountAndLikeCountSectionController");
    hSections += IFZeroSection("IGVideoViewCountSectionController");
    hSections += IFZeroSection("IGVideoPlayCountSectionController");
    int hSplash = IFSwizzle(c_vc, "viewWillAppear:", IFWillAppear, (void **)&orig_willAppear);
    IFSwizzle(objc_getClass("UIScrollView"), "setScrollEnabled:", IFSetScroll, (void **)&orig_setScroll);
    int hViewer = IFSwizzle(c_vc, "presentViewController:animated:completion:", IFPresent, (void **)&orig_present);
    hViewer += IFSwizzle(objc_getClass("UINavigationController"), "pushViewController:animated:", IFPush, (void **)&orig_push);

    IFLoadSplashImage();
    int hImage = IFSwizzleClass(objc_getClass("UIImage"), "imageNamed:", IFImageNamed, (void **)&orig_imageNamed);
    hImage += IFSwizzleClass(objc_getClass("UIImage"), "imageNamed:inBundle:compatibleWithTraitCollection:", IFImageNamedBundle, (void **)&orig_imageNamedBundle);

    IFDiscoverAccessGroup();
    IFHookAccessGroup("FBKeychainItemController");
    IFHookAccessGroup("UICKeyChainStore");

    g_ready = 1;
    syslog(LOG_NOTICE, "[instagram-mod] ready tabs=%d hide=%d likes=%d sections=%d splash=%d viewer=%d image=%d keychain=%d",
        hTabs, hHide, hLikes, hSections, hSplash, hViewer, hImage, g_accessGroup ? 1 : 0);
}

static void IFLater(void *ctx) {
    (void)ctx;
    void *pool = objc_autoreleasePoolPush();
    IFInstall();
    objc_autoreleasePoolPop(pool);
}

static void IFInstallEarly(void) {
    dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_LAZY);
    s_isKind = sel_registerName("isKindOfClass:");
    c_string = objc_getClass("NSString");
    IFPrepareGroupURL();
    IFSwizzle(objc_getClass("NSFileManager"), "containerURLForSecurityApplicationGroupIdentifier:", IFContainerURL, (void **)&orig_containerURL);

    void *lib = dlopen("/usr/lib/libSystem.B.dylib", RTLD_LAZY);
    void (*async_f)(void *, void *, void (*)(void *)) = lib ? (void (*)(void *, void *, void (*)(void *)))dlsym(lib, "dispatch_async_f") : NULL;
    void *(*mainq)(void) = lib ? (void *(*)(void))dlsym(lib, "dispatch_get_main_queue") : NULL;
    if (async_f && mainq) async_f(mainq(), NULL, IFLater);
    else IFLater(NULL);
}

__attribute__((constructor)) static void IFCtor(void) {
    void *pool = objc_autoreleasePoolPush();
    IFInstallEarly();
    objc_autoreleasePoolPop(pool);
}
