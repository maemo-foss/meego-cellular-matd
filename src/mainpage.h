/**
 * @mainpage
 *
 * libmatd is a plugin-based library to interpret AT commands.
 *
 * The interpreter can be started and stopped with a very simple API,
 * see @ref external.
 *
 * When started, the interpreter first scans its plugin directory.
 * Each plugin can register a list of AT command that it can handle using
 * the @ref cmd plugin API.
 * The interpreter will then accept AT commands from the terminal equipment,
 * and execute the plugin-registered command handlers as needed.
 *
 * When executing commands, plugins can read data from (rarely needed)
 * and write data to (often needed) the terminal equipment using the
 * @ref io. Also, in some cases, asynchronous ("unsolicited") messages must be
 * sent to the terminal; the affected plugins are responsible for creating the
 * necessary thread or ensuring that the adequate main loop is running.
 *
 * @author Rémi Denis-Courmont
 */
