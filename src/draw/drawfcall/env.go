package drawfcall

// getenv returns the value of the environment variable name in env, whose
// entries are "name=value" pairs. Later entries override earlier ones of the
// same name — matching os/exec's treatment of Cmd.Env — so appending to a
// base environment (usually os.Environ) overrides it.
func getenv(env []string, name string) string {
	v := ""
	for _, kv := range env {
		if len(kv) > len(name) && kv[len(name)] == '=' && kv[:len(name)] == name {
			v = kv[len(name)+1:]
		}
	}
	return v
}
