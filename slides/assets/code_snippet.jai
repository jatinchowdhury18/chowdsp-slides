#import "Basic";

greet :: (name: string) {
    print("Hello, %!\n", name);
}

main :: () {
    x: s32 = 12; // comment
    greet("world");
}
