func Bench<T>(
    name: String = #function,
    file: String = #function,
    line: Int = #line,
    closure: () throws -> T
) rethrows -> T {
#if !NDEBUG
    // time
    defer {
        // must defer incase the function throws
        // end timing
    }
#endif
    return try closure()
}

func helloWorld() {
    Bench {
        print("Hello, world!")
    }
}
