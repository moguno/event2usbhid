#ifndef USBG_TEST_H
#define USBG_TEST_H

#include <usbg/usbg.h>
#include <usbg/function/ms.h>
#include <usbg/function/net.h>
#include <usbg/function/ffs.h>
#include <usbg/function/phonet.h>
#include <usbg/function/midi.h>

#include <sys/queue.h>
#include "usbg/usbg_internal.h"

/* Simple structures for defining gadgets. All arrays should be null-terminated.*/

/**
 * @file tests/usbg-test.h
 */

struct test_function
{
	usbg_function_type type;
	char *instance;

	char *path;
	char *name;
	void *attrs;
	int writable;
};

struct test_binding
{
	struct test_function *target;
	char *name;
	int writable;
};

struct test_config
{
	char *label;
	int id;
	struct test_function *bound_funcs;

	struct test_binding *bindings;
	char *path;
	char *name;
	int writable;
	struct usbg_config_strs *strs;
	struct usbg_config_attrs *attrs;
};

struct test_gadget
{
	char *name;
	char *udc;
	struct test_config *configs;
	struct test_function *functions;

	char *path;
	int writable;
};

struct test_state
{
	char *configfs_path;
	/* filled by prepare_state() */
	char *path;
	struct test_gadget *gadgets;
	char **udcs;
	int writable;
};

typedef enum {
	STR_MNF = 0,
	STR_PRD,
	STR_SER,
	GADGET_STR_MAX
} gadget_str;

typedef enum {
	MAX_POWER = 0,
	BM_ATTRIBUTES,
	CONFIG_ATTR_MAX
} config_attr;

typedef enum {
	FORMAT_HEX,
	FORMAT_DEC
} attr_format;

#define TEST_FUNCTION_LIST_END { \
		.instance = NULL, \
	}

#define TEST_CONFIG_LIST_END { \
		.label = NULL, \
		.bindings = NULL, \
	}

#define TEST_GADGET_LIST_END { \
		.name = NULL, \
		.udc = NULL, \
		.configs = NULL, \
		.functions = NULL, \
	}

#define expect_path(function, param, data) \
	expect_check(function, param, \
		     (CheckParameterValue)(path_equal_display_error), data)

/**
 * @brief Prepare given state for using in tests
 * @details Generate full pathes to state elements and sort state's content.
 * Must be called before pasing state to push_* and pull_* functions.
 * @param[in] state State schema used to genrate test state
 * @return Pointer to state which can be used for testing. Returned value is
 * equal to #state if writable attribute has been set to 1 or pointer
 * to newly allocated test_state filled with suitable values. All memory
 * allocated in this function is scheduled to free using free_later().
 */
struct test_state *prepare_state(struct test_state *state);

/**
 * @brief Prepare given config for using in tests
 * @details Generate required pathes for given config and sort content
 * (i.e. binding list)
 * @param[in] c Config to be filled out
 * @param[in] cpath Path to configs directory
 * @param[in] fpath Path to functions directory
 */
void prepare_config(struct test_config *c, char *cpath, char *fpath);

/**
 * @brief Prepare given function for using in tests
 * @details Generate required pathes for given function
 * @param[in] f Function to be filled out
 * @param[in] path Path to functions directory
 */
void prepare_function(struct test_function *f, char *path);

/**
 * @brief Prepare given gadget for using in tests
 * @details Generate required paths for given gadget and sort it's content
 * (i.e. functions list and config list)
 * @param[in] state Pointer to gadget's parent state
 * @param[in] g Gadget to be filled out
 */
void prepare_gadget(struct test_state *state, struct test_gadget *g);

/**
 * @brief Fill given binding with required values
 * @details Make given binding point to a function
 * @param[in] b Test binding to be prepared
 * @param[in] f Function to which binding will point
 * @param[in] fpath Path to functions directory
 */
void prepare_binding(struct test_binding *b, struct test_function *f, char *fpath);

/**
 * @brief Prepare fake filesystem to init usbg with given test state
 * @details Use wrapped i/o functions to simulate configfs state for usbg.
 * Calling usbg_init without preparation and with mocked i/o functions
 * may fail.
 * @param[in] state Fake state of configfs defined in test
 */
void push_init(struct test_state *state);

/**
 * Prepare specific attributes writting/reading
 **/

/**
 * @brief Prepare for getting config attributes
 * @param[in] config Configuration from which attributes will be get
 * @param[in] attrs Attributes which will be present in virtual filesystem
 */
void push_config_attrs(struct test_config *config,
		       struct usbg_config_attrs *attrs);

/**
 * @brief Preapre for setting config attributes
 * @param[in] config Configuration on which attributes will be set
 * @param[in] attrs Attributes which will be set on given config
 */
void pull_config_attrs(struct test_config *config,
		       struct usbg_config_attrs *attrs);

/**
 * @brief Get gadget attribute
 * @param[in] attrs
 * @param[in] attr
 */
int get_gadget_attr(struct usbg_gadget_attrs *attrs, usbg_gadget_attr attr);

/**
 * @brief Prepare to write given attribute by libusbg
 * @param[in] gadget Test gadget related to given attribute
 * @param[in] attr Attribute
 * @param[in] value Attributes value
 **/
void push_gadget_attribute(struct test_gadget *gadget,
			   usbg_gadget_attr attr, int value);

/**
 * @brief Prepare to read given attribute by libusbg
 * @param[in] gadget Test gadget related to given attribute
 * @param[in] attr Attribute
 * @param[in] value Attributes value
 **/
void pull_gadget_attribute(struct test_gadget *gadget,
			   usbg_gadget_attr attr, int value);

/**
 * @brief Prepare fake filesystem to get given gadget attributes
 * @details Prepare queue of values passed to wrapped i/o functions,
 * all values got from given attributes structure.
 * @param[in] gadget Pointer to gadget
 * @param[in] attrs Pointer to attributes which gadget should have
 * @warning Calling usbg_get_gadget_attrs function whithout this
 * preparation and with wrapped i/o may fail.
 */
void push_gadget_attrs(struct test_gadget *gadget,
		       struct usbg_gadget_attrs *attrs);

/**
 * @brief Prepare fake filesystem for attributes setting attempt.
 * @details Prepare queue of values passed to wrapped i/o functions,
 * corresponding to functions called on attributes setting
 * @param[in] gadget Pointer to gadget
 * @param[in] attrs Pointer to expected attributes
 * @warning Calling usbg_get_gadget_attrs function whithout this
 * preparation and with wrapped i/o may fail.
 */
void pull_gadget_attrs(struct test_gadget *gadget,
		       struct usbg_gadget_attrs *attrs);

/**
 * @brief Prepare fake filesystem to get given function attributes
 * @details Prepare queue of values passed to wrapped i/o functions,
 * all values got from given attributes structure.
 * @warning Calling usbg_get_function_attrs function whithout this
 * preparation and with wrapped i/o may fail.
 */
void push_function_attrs(struct test_function *func, void *attrs);

/**
 * @brief Prepare fake filesystem to set given function attributes
 * @details Prepare queue of values passed to wrapped i/o functions,
 * all values got from given attributes structure.
 * @warning Calling usbg_set_function_attrs function whithout this
 * preparation and with wrapped i/o may fail.
 */
void pull_function_attrs(struct test_function *func, void *attrs);

/**
 * @brief Get gadget string
 * @param[in] strs Set of gadget strings
 * @param[in] str Identifier of string which should be returned
 * @return Selected string from given set of strings
 */
const char *get_gadget_str(struct usbg_gadget_strs *strs, gadget_str str);

/**
 * @brief Prepare filesystem to set selected gadget string
 * @param[in] gadget Gadget on which str will be set
 * @param[in] lang Language of string
 * @param[in] str String identifier
 * @param[in] content String expected to be set
 */
void pull_gadget_string(struct test_gadget *gadget, int lang,
			gadget_str str, const char *content);

/**
 * @brief Prepare filesystem to set given gadget strings
 * @param[in] gadget Gadget on which strings will be set
 * @param[in] lang Language of strings
 * @param[in] strs Strings expected to be set
 */
void pull_gadget_strs(struct test_gadget *gadget, int lang,
		      struct usbg_gadget_strs *strs);

/**
 * @brief prepare for reading gadget's strings
 */
void push_gadget_strs(struct test_gadget *gadget, int lang,
		      struct usbg_gadget_strs *strs);

/**
 * @brief Prepare for /ref usbg_set_config_string calling
 * @details Expect setting the same string as given one
 * @param[in] config on which strings will be set
 * @param[in] lang Language of strings
 * @param[in] str string to be set as configuration string
 */
void pull_config_string(struct test_config *config, int lang, const char *str);

/**
 * @brief Prepare for writting given config strings
 * @param[in] config on which strings will be set
 * @param[in] lang Language of strings
 * @param[in] strs Strings expected to be set
 */
void pull_config_strs(struct test_config *config, int lang,
		      struct usbg_config_strs *strs);

/**
 * @brief Prepare for /ref usbg_get_config_string calling
 * @details Expect setting the same string as given one
 * @param[in] config from which strings will be get
 * @param[in] lang Language of strings
 * @param[in] str string which should be returned as configuration string
 */
void push_config_string(struct test_config *config, int lang, const char *str);

/**
 * @brief Prepare for reading config strings
 * @param[in] config from which strings will be get
 * @param[in] lang Language of strings
 * @param[in] strs Strings which should be returned
 */
void push_config_strs(struct test_config *config, int lang,
		      struct usbg_config_strs *strs);

/**
 * @brief Prepare for creating config
 * @param[in] tc Test config to be created
 */
void pull_create_config(struct test_config *tc);

/**
 * @brief Prepare for creating function
 * @param[in] tf Test function to be created
 */
void pull_create_function(struct test_function *tf);

/**
 * @brief Copy state without configs and functions
 * @param[in] ts State to bo copied
 * @return State with empty gadgets
 */
struct test_state *build_empty_gadget_state(struct test_state *ts);

/**
 * @brief Store given pointer on cleanup stack
 * @details All stacked pointers will be freed by calling cleanup_queue.
 * This can be used to manage memory needed for single test casees.
 */
void free_later(void *ptr);

/**
 * @brief Cleans up memory no longer needed
 * @details Frees all pointer stored on cleanup stack by calling free_later
 * @warning Calling this function before end of single test usually make test state
 * unusable. Use it only when you no longer need allocated data (at the end of
 * test case, in most cases)
 */
void cleanup_stack();

/**
 * @brief init usbg with given test state
 */
void init_with_state(struct test_state *in, usbg_state **out);

/**
 * @brief Safely initialize usbg state from pointer given to test
 * @param[in] state Pointer given to test function
 * @param[out] ts Pointer to be filled with test state
 * @param[out] s Pointer to be filled with usbg state
 */
void safe_init_with_state(void **state, struct test_state **ts, usbg_state **s);

/**
 * @brief Assert that given config attributes are equal
 */
void assert_config_attrs_equal(struct usbg_config_attrs *actual,
			       struct usbg_config_attrs *expected);

/**
 * @brief Assert that given usbg binding matches given test binding
 * @param[in] f Pointer to usbg binding struct
 * @param[in] expected Pointer to test binding struct with expected values
 */
void assert_binding_equal(usbg_binding *b, struct test_binding *expected);

/**
 * @brief Assert that given usbg function matches given test function
 * @param[in] f Pointer to usbg function struct
 * @param[in] expected Pointer to test function struct with expected values
 */
void assert_func_equal(usbg_function *f, struct test_function *expected);

/**
 * @brief Assert that given usbg config matches given test config
 * @param[in] c Pointer to usbg config struct
 * @param[in] expected Pointer to test config struct with expected values
 */
void assert_config_equal(usbg_config *c, struct test_config *expected);

/**
 * @brief Assert that given usbg gadget matches given test gadget
 * @param[in] g Pointer to usbg gadget struct
 * @param[in] expected Pointer to test gadget struct with expected values
 */
void assert_gadget_equal(usbg_gadget *g, struct test_gadget *expected);

/**
 * @brief Assert that given usbg state matches given test state
 * @param[in] s Pointer to usbg state struct
 * @param[in] expected Pointer to test state struct with expected values
 */
void assert_state_equal(usbg_state *s, struct test_state *expected);

/**
 * @brief Compare path names
 * @details Given pathes don't need to exist
 * @return Integer less than, equal to, or greater than zero if a is (respectively)
 * less than, equal to, or greater than b.
 */
int path_cmp(const char *a, const char *b);

/**
 * @brief Print error when given paths are not equal
 * @return 1 if paths are equal, 0 otherwise
 * @note Argument type is defined by cmocka. This specific function type is defined
 * as custom comparing function in cmocka framework.
 */
int path_equal_display_error(const LargestIntegralType actual, const LargestIntegralType expected);

/**
 * @brief Compare attributes (as strings)
 * @return Integer less than, equal to, or greater than zero if a is (respectively)
 * less than, equal to, or greater than b.
 */
int hex_str_cmp(const char *actual, const char *expected);

/**
 * @brief Print error when given attributes are not equal
 * @return 1 if attributes are equal, 0 otherwise
 * @note Argument type is defined by cmocka. This specific function type is defined
 * as custom comparing function in cmocka framework.
 */
int hex_str_equal_display_error(const LargestIntegralType actual, const LargestIntegralType expected);

/**
 * @brief Assert that given path strings are equal
 * @details Given pathes don't need to exist
 */
void assert_path_equal(const char *actual, const char *expected);

/**
 * @brief Assert that given usbg gadget attributes sets are equal
 * @param[in] actual Pointer to actual gadget attributes structure
 * @param[in] expected Pointer to expeced gadget attributes structure
 */
void assert_gadget_attrs_equal(struct usbg_gadget_attrs *actual,
			       struct usbg_gadget_attrs *expected);

/**
 * @brief Assert that given function attributes are the same.
 * @param[in] actual Pointer to actual attributes object
 * @param[in] expected Pointer to expected attributes obejct
 * @param[in] type Type of function, which attributes are checked
 */
void assert_function_attrs_equal(void *actual, void *expected,
				 usbg_function_type type);

/**
 * @brief Assert that given gadget strings are equal
 */
void assert_gadget_strs_equal(struct usbg_gadget_strs *actual,
			      struct usbg_gadget_strs *expected);

/**
 * @brief Function that performs some test on given usbg function
*/
typedef void (*FunctionTest)(usbg_function *f, struct test_function *tf);

/**
 * @brief Call given function for all usb functions present in given state
 * @param[in] ts Test state to be tested
 * @param[in] s Properly prepared usbg state to be tested
 * @param[in] fun Function to be called on each usb function in state
 */
void for_each_test_function(struct test_state *ts, usbg_state *s, FunctionTest fun);

/**
 * @brief Function that performs some test on given usbg config
*/
typedef void (*ConfigTest)(usbg_config *c, struct test_config *tc);

/**
 * @brief Call given function for all usb configs present in given state
 * @param[in] ts Test state to be tested
 * @param[in] s Properly prepared usbg state to be tested
 * @param[in] fun Function to be called on each usb function in state
 */
void for_each_test_config(struct test_state *ts, usbg_state *s, ConfigTest fun);

/**
 * @brief Function that performs test on given usbg binding
 */
typedef void (*BindingTestFunc)(struct test_binding *tb, usbg_binding *b);

/**
 * @brief Call given function for all usb bindings present in given state
 * @param[in] ts Test state to be tested
 * @param[in] s Properly prepared usbg state to be tested
 * @param[in] fun Function to be called on each usb binding in state
 */
void for_each_binding(struct test_state *ts, usbg_state *s, BindingTestFunc fun);

/**
 * @brief Function that performs test on given usbg gadget
 */
typedef void (*GadgetTestFunc)(usbg_gadget *g, struct test_gadget *tg);

/**
 * @brief Call given function for all usb gadgets present in given state
 * @param[in] ts Test state to be tested
 * @param[in] s Properly prepared usbg state to be tested
 * @param[in] fun Function to be called on each usb gadget in state
 */
void for_each_test_gadget(struct test_state *ts, usbg_state *s, GadgetTestFunc fun);

static inline void *safe_calloc(int count, size_t size)
{
	void *ptr;

	ptr = calloc(count, size);
	if (ptr == NULL)
		fail();

	free_later(ptr);
	return ptr;
}

static inline void *safe_malloc(size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (ptr == NULL)
		fail();

	free_later(ptr);
	return ptr;
}

static inline int safe_asprintf(char **ptr, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vasprintf(ptr, fmt, args);
	va_end(args);

	if (ret < 0)
		fail();

	free_later(*ptr);
	return ret;
}

#endif /* USBG_TEST_H */
