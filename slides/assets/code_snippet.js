// greeting module
class Greeter {
    constructor(name) {
        this.name = name;
    }

    greet() {
        return `Hello, ${this.name}!`;
    }
}

const g = new Greeter("world");
console.log(g.greet());
