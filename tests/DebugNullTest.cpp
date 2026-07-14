// Intentionally empty. Used briefly for one-off diagnostic printf-style
// debugging while tracking down a null-test/Output-trim interaction (root
// cause turned out to be a bug in the *test*, not the engine - see the
// "Engine null test" TEST_CASE in EngineTests.cpp, which now sets Output to
// 0 dB so the null assertion isn't confounded by the Output trim stage).
// Left as an empty translation unit rather than removed, since file
// deletion isn't available in the environment this was authored in; it
// contributes no test cases.
