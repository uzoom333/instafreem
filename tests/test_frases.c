// Testa no PC as regras que apagam números e frases de curtida.
// Inclui o tweak inteiro e troca o runtime do iOS por funções vazias.
#include "../src/instagram_mod.c"

int printf(const char *, ...);

id objc_msgSend(id a, SEL b, ...) { (void)a; (void)b; return nil; }
Class objc_getClass(const char *n) { (void)n; return NULL; }
SEL sel_registerName(const char *n) { (void)n; return NULL; }
Method class_getInstanceMethod(Class c, SEL s) { (void)c; (void)s; return NULL; }
Method class_getClassMethod(Class c, SEL s) { (void)c; (void)s; return NULL; }
IMP method_getImplementation(Method m) { (void)m; return NULL; }
IMP method_setImplementation(Method m, IMP i) { (void)m; (void)i; return NULL; }
const char *object_getClassName(id o) { (void)o; return NULL; }
id objc_retain(id o) { return o; }
void objc_release(id o) { (void)o; }
Class class_getSuperclass(Class c) { (void)c; return NULL; }
Class object_getClass(id o) { (void)o; return NULL; }
void *objc_autoreleasePoolPush(void) { return NULL; }
void objc_autoreleasePoolPop(void *p) { (void)p; }

static int falhas;

static void caso(const char *texto, int esperado) {
    int obtido = IFLikePhrase(texto);
    if (obtido != esperado) {
        printf("FALHOU: \"%s\" deu %d, esperava %d\n", texto, obtido, esperado);
        falhas++;
    }
}

int main(void) {
    // contagens e linhas de curtida: somem
    caso("12 curtidas", 1);
    caso("1.234 likes", 1);
    caso("3 comentários", 1);
    caso("Curtido por fulano e outras pessoas", 1);
    caso("Liked by fulano and others", 1);
    caso("Patrocinado", 1);
    caso("Sponsored", 1);
    // texto de conversa e legenda: fica
    caso("isso é publicidade", 0);
    caso("post patrocinado pela loja", 0);
    caso("vi com outras pessoas ontem", 0);
    caso("eu curtido por você kkk", 0);
    caso("curtidas", 0);
    caso("bom dia", 0);
    if (falhas) return 1;
    printf("frases: ok\n");
    return 0;
}
