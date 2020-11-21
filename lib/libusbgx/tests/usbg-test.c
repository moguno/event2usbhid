#include <usbg/usbg.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include <time.h>

#include "usbg-test.h"

static struct simple_stack{
	void *ptr;
	struct simple_stack *next;
} *cleanup_top = NULL;

static const char *gadget_str_names[] = {
	"manufacturer",
	"product",
	"serialnumber",
};

static const char *config_attr_names[] = {
	"MaxPower",
	"bmAttributes"
};

static attr_format config_attr_format[] = {
	[MAX_POWER] = FORMAT_DEC,
	[BM_ATTRIBUTES] = FORMAT_HEX
};

void free_later(void *ptr)
{
	struct simple_stack *t;

	t = malloc(sizeof(*t));
	t->ptr = ptr;
	t->next = cleanup_top;
	cleanup_top = t;
}

void cleanup_stack()
{
	struct simple_stack *t;

	while (cleanup_top) {
		free(cleanup_top->ptr);
		t = cleanup_top->next;
		free(cleanup_top);
		cleanup_top = t;
	}
}

/* Represent last file/dir opened, next should have bigger numbers.*/
static int file_id = 0;
static int dir_id = 0;

#define PUSH_FILE(file, content, len) do {\
	file_id++;\
	expect_path(fopen, path, file);\
	will_return(fopen, file_id);\
	expect_value(fread, stream, file_id);\
	will_return(fread, content);\
	will_return(fread, len);\
	expect_value(fclose, fp, file_id);\
	will_return(fclose, 0);\
} while(0)

#define PUSH_FILE_STR(file, content) \
	PUSH_FILE(file, content, strlen(content) + 1);

#define PUSH_EMPTY_DIR(p) do {\
	expect_string(scandir, dirp, p);\
	will_return(scandir, 0);\
} while(0)

#define EXPECT_OPENDIR(n) do {\
	dir_id++;\
	expect_path(opendir, name, n);\
	will_return(opendir, 0);\
	will_return(opendir, dir_id);\
	expect_value(closedir, dirp, dir_id);\
	will_return(closedir, 0);\
} while(0)

#define EXPECT_OPENDIR_ERROR(n, e) do {\
	expect_path(opendir, name, n);\
	will_return(opendir, e);\
	will_return(opendir, NULL);\
} while(0)

#define PUSH_DIR(p, c) do {\
	expect_path(scandir, dirp, p);\
	will_return(scandir, c);\
} while(0)

#define PUSH_DIR_ENTRY(name, type) do {\
	will_return(scandir, name);\
	will_return(scandir, type);\
	will_return(scandir, 1);\
} while(0)

#define PUSH_LINK(p, c, len) do {\
	expect_path(readlink, path, p);\
	expect_in_range(readlink, bufsiz, len, INT_MAX);\
	will_return(readlink, c);\
} while(0)

#define EXPECT_WRITE(file, content, len) do {	\
	file_id++;\
	expect_path(fopen, path, file);\
	will_return(fopen, file_id);\
	expect_value(fwrite, stream, file_id);\
	expect_memory(fwrite, ptr, content, len); \
	will_return(fwrite, len);\
	expect_value(fclose, fp, file_id);\
	will_return(fclose, 0);\
} while(0)

#define EXPECT_WRITE_STR(file, content)\
	EXPECT_WRITE(file, content, strlen(content) + 1)

#define EXPECT_HEX_WRITE(file, content) do {\
	file_id++;\
	expect_path(fopen, path, file);\
	will_return(fopen, file_id);\
	expect_value(fwrite, stream, file_id);\
	expect_check(fwrite, ptr, hex_str_equal_display_error, content);\
	will_return(fwrite, 0);\
	expect_value(fclose, fp, file_id);\
	will_return(fclose, 0);\
} while(0)

#define EXPECT_MKDIR(p) do {\
	expect_path(mkdir, pathname, p);\
	expect_value(mkdir, mode, 00777);\
	will_return(mkdir, 0);\
} while(0)

/**
 * @brief Compare test gadgets' names
 */
static int test_gadget_cmp(struct test_gadget *a, struct test_gadget *b)
{
	return strcoll(a->name, b->name);
}

/**
 * @brief Compare test functions' names
 */
static int test_function_cmp(struct test_function *a, struct test_function *b)
{
	return strcoll(a->name, b->name);
}

/**
 * @brief Compare test bindings' names
 */
static int test_binding_cmp(struct test_binding *a, struct test_binding *b)
{
	return strcoll(a->name, b->name);
}

/**
 * @brief Compare test configs' names
 */
static int test_config_cmp(struct test_config *a, struct test_config *b)
{
	return strcoll(a->name, b->name);
}

void prepare_binding(struct test_binding *b, struct test_function *f,
		     char *fpath)
{
	if (!f->name)
		prepare_function(f, fpath);

	if (!b->name) {
		b->name = strdup(f->name);
		if (b->name == NULL)
			fail();
		free_later(b->name);
	}

	b->target = f;
}

void prepare_config(struct test_config *c, char *cpath, char *fpath)
{
	int count = 0;
	struct test_function *f;
	struct test_binding *b;
	int i;

	safe_asprintf(&c->name, "%s.%d", c->label, c->id);

	c->path = cpath;

	/* check if bindings has been already filled */
	if (!c->bindings) {
		for (f = c->bound_funcs; f->instance; f++)
			count++;

		c->bindings = safe_calloc(count + 1, sizeof(*c->bindings));
	} else {
		for (b = c->bindings; b->name; b++)
			count++;
	}

	for (i = 0; i < count; i++)
		prepare_binding(&c->bindings[i], &c->bound_funcs[i], fpath);

	qsort(c->bindings, count, sizeof(*c->bindings),
		(int (*)(const void *, const void *))test_binding_cmp);

}

void prepare_function(struct test_function *f, char *path)
{
	const char *func_type;

	func_type = usbg_get_function_type_str(f->type);
	if (func_type == NULL)
		fail();

	safe_asprintf(&f->name, "%s.%s", func_type, f->instance);

	f->path = path;
}

void prepare_gadget(struct test_state *state, struct test_gadget *g)
{
	struct test_config *c;
	struct test_function *f;
	char *fpath;
	char *cpath;
	int count;

	g->path = strdup(state->path);
	if (!g->path)
		fail();

	free_later(g->path);

	safe_asprintf(&fpath, "%s/%s/functions", g->path, g->name);

	count = 0;
	for (f = g->functions; f->instance; f++) {
		prepare_function(f, fpath);
		count++;
	}

	/* Path needs to be known somehow when list is empty */
	f->path = fpath;

	qsort(g->functions, count, sizeof(*g->functions),
		(int (*)(const void *, const void *))test_function_cmp);

	safe_asprintf(&cpath, "%s/%s/configs", g->path, g->name);

	count = 0;
	for (c = g->configs; c->label; c++) {
		prepare_config(c, cpath, fpath);
		count++;
	}

	/* Path needs to be known somehow when list is empty */
	c->path = cpath;

	qsort(g->configs, count, sizeof(*g->configs),
		(int (*)(const void *, const void *))test_config_cmp);

}

static void cpy_test_function(struct test_function *to,
			      struct test_function *from)
{
	/* Reuse instance */
	to->instance = from->instance;
	to->type = from->type;
        /* path and name is not being copied because
	   it has not been allocated now */

	to->writable = 1;
}

static struct test_function *dup_test_functions(struct test_function *functions)
{
	struct test_function *f, *nf, *new_functions;
	int count = 0;

	for (f = functions; f->instance; ++f)
		++count;

	new_functions = safe_calloc(count + 1, sizeof(*f));

	for (f = functions, nf = new_functions; f->instance; ++f, ++nf)
		cpy_test_function(nf, f);

	return new_functions;
}

static struct test_function *get_new_binding_target(struct test_function *which,
						    struct test_function *old,
						    int count,
						    struct test_function *new)
{
	struct test_function *ret = NULL;

	/* Should duplicate function? */
	if (which < old || ((which - old) > count)) {
		/* We may need to do a deep copy */
		if (!which->writable) {
			ret = safe_calloc(1, sizeof(*ret));
			cpy_test_function(ret, which);
		} else {
			ret = which;
		}
	} else if (old != new) {
		/* Function has been copied in bound_funcs so just
		   set new address */
		ret = which - old + new;
	} else {
		/* Functions are reused so leave address as is */
		ret = which;
	}

	return ret;
}

static void cpy_test_binding(struct test_binding *to,
			     struct test_binding *from,
			     struct test_function *old,
			     int func_count,
			     struct test_function *new)
{
	/* Reuse name */
	to->name = from->name;
	to->target = get_new_binding_target(from->target, old, func_count, new);

	to->writable = 1;
}

static struct test_binding *dup_test_bindings(struct test_binding *bindings,
					     struct test_function *old,
					     int func_count,
					     struct test_function *new)
{
	struct test_binding *b, *nb, *new_bindings;
	int count = 0;

	for (b = bindings; b->name; ++b)
		++count;

	new_bindings = safe_calloc(count + 1, sizeof(*b));

	for (b = bindings, nb = new_bindings; b->name; ++b, ++nb)
		cpy_test_binding(nb, b, old, func_count, new);

	return new_bindings;
}

static void cpy_test_config(struct test_config *to,
			    struct test_config *from)
{
	int func_count = 0;
	struct test_function *f;
	struct test_binding *b;

	/* Reuse label */
	to->label = from->label;
	to->id = from->id;
	to->strs = from->strs;
	to->attrs = from->attrs;

	if (from->bound_funcs) {
		/* If at least one function is not writable
		   we have to copy all of them */
		for (f = from->bound_funcs; f->instance; ++f) {
			++func_count;
			if (!f->writable && !to->bound_funcs) {
				to->bound_funcs =
					dup_test_functions(from->bound_funcs);
			}
		}

		if (!f->name && !to->bound_funcs)
			to->bound_funcs = from->bound_funcs;
	}

	/* If bindings are set copy also them */
	if (from->bindings) {
		/* If at least one function is not writable
		   we have to copy all of them */
		for (b = from->bindings; b->name; ++b)
			if (!b->writable)
				to->bindings =
					dup_test_bindings(from->bindings,
							  from->bound_funcs,
							  func_count,
							  to->bound_funcs);

		/* if we are reusing binding we have to translate target
		   address to new one which is writable */
		if (!b->name && !to->bindings) {
			to->bindings = from->bindings;
			for (b = from->bindings; b->name; ++b)
				b->target =
					get_new_binding_target(
						b->target,
						from->bound_funcs,
						func_count,
						to->bound_funcs);
		}
	}

	to->writable = 1;
}

static struct test_config *dup_test_configs(struct test_config *configs)
{
	struct test_config *c, *nc, *new_configs;
	int count = 0;

	for (c = configs; c->label; ++c)
		++count;

	new_configs = safe_calloc(count + 1, sizeof(*c));

	for (c = configs, nc = new_configs; c->label; ++c, ++nc)
		cpy_test_config(nc, c);

	return new_configs;
}

static void cpy_test_gadget(struct test_gadget *to, struct test_gadget *from)
{
	struct test_function *f;
	struct test_config *c;

	/* Reuse name and udc */
	to->name = from->name;
	to->udc = from->udc;
        /* path is not being copied because it has not been allocated */

	/* If at least one function is not writable
	   we have to copy all of them */
	for (f = from->functions; f->instance; ++f)
		if (!f->writable) {
			to->functions =
				dup_test_functions(from->functions);
			break;
		}

	if (!f->name && !to->functions)
		to->functions = from->functions;


	/* If at least one config is not writable
	   we have to copy all of them */
	for (c = from->configs; c->label; ++c)
		if (!c->writable) {
			to->configs = dup_test_configs(from->configs);
			break;
		}

	if (!c->name && !to->configs)
		to->configs = from->configs;

	to->writable = 1;
}

static struct test_gadget *dup_test_gadgets(struct test_gadget *gadgets)
{
	struct test_gadget *g, *ng, *new_gadgets;
	int count = 0;

	for (g = gadgets; g->name; ++g)
		++count;

	new_gadgets = safe_calloc(count + 1, sizeof(*g));

	for (g = gadgets, ng = new_gadgets; g->name; ++g, ++ng)
		cpy_test_gadget(ng, g);

	return new_gadgets;
}

static struct test_state *dup_test_state(struct test_state *state)
{
	struct test_state *new_state;
	struct test_gadget *g;

	new_state = safe_calloc(1, sizeof(*new_state));

	/* We don't copy configfs path because it is never changed
	 if you would like to free it before test end replace
	 this code with strdup */
	new_state->configfs_path = state->configfs_path;

	/* path is not being copied because it has not been allocated */

	/* If at least one gadget is not writable we have to copy all of them */
	for (g = state->gadgets; g->name; ++g)
		if (!g->writable) {
			new_state->gadgets =
				dup_test_gadgets(state->gadgets);
			break;
		}

	if (!g->name && !new_state->gadgets)
		new_state->gadgets = state->gadgets;

	/* udcs are also never changed so leave them as they are */
	new_state->udcs = state->udcs;

	new_state->writable = 1;

	return new_state;
}

struct test_state *prepare_state(struct test_state *state)
{
	struct test_gadget *g;
	struct test_state *new_state;
	int count = 0;

	if (!state->writable)
		new_state = dup_test_state(state);
	else
		new_state = state;

	safe_asprintf(&(new_state->path), "%s/usb_gadget",
		      new_state->configfs_path);

	for (g = new_state->gadgets; g->name; g++) {
		prepare_gadget(new_state, g);
		count++;
	}

	qsort(new_state->gadgets, count, sizeof(*new_state->gadgets),
		(int (*)(const void *, const void *))test_gadget_cmp);

	return new_state;
}

struct test_state *build_empty_gadget_state(struct test_state *ts)
{
	struct test_state *ret;
	struct test_gadget *tg;
	int count = 0;

	ret = safe_malloc(sizeof(*ret));
	ret->udcs = ts->udcs;
	ret->configfs_path = ts->configfs_path;

	for (tg = ts->gadgets; tg->name; ++tg)
		count++;

	ret->gadgets = safe_calloc(count+1, sizeof(*ts->gadgets));
	memcpy(ret->gadgets, ts->gadgets, count*sizeof(*ts->gadgets));

	for (tg = ret->gadgets; tg->name; ++tg) {
		tg->configs = safe_calloc(1, sizeof(*tg->configs));
		tg->functions = safe_calloc(1, sizeof(*tg->functions));
	}

	return prepare_state(ret);
}

/* Simulation of configfs for init */

static void push_binding(struct test_config *conf, struct test_binding *binding)
{
	char *s_path;
	char *d_path;

	safe_asprintf(&s_path, "%s/%s/%s",
		      conf->path, conf->name, binding->name);
	safe_asprintf(&d_path, "%s/%s",
		      binding->target->path, binding->target->name);

	PUSH_LINK(s_path, d_path, USBG_MAX_PATH_LENGTH - 1);
}

static void push_config(struct test_config *c)
{
	struct test_binding *b;
	int count = 0;
	char *path;

	safe_asprintf(&path, "%s/%s", c->path, c->name);

	for (b = c->bindings; b->name; b++)
		count++;

	PUSH_DIR(path, count);
	for (b = c->bindings; b->name; b++) {
		PUSH_DIR_ENTRY(b->name, DT_LNK);
		push_binding(c, b);
	}
}

static void push_gadget(struct test_gadget *g)
{
	int count;
	struct test_config *c;
	struct test_function *f;
	char *path, *os_desc_path;

	safe_asprintf(&path, "%s/%s/UDC", g->path, g->name);
	PUSH_FILE_STR(path, g->udc);

	count = 0;
	for (f = g->functions; f->instance; f++)
		count++;

	PUSH_DIR(f->path, count);
	for (f = g->functions; f->instance; f++)
		PUSH_DIR_ENTRY(f->name, DT_DIR);

	count = 0;
	for (c = g->configs; c->label; c++)
		count++;

	PUSH_DIR(c->path, count);
	for (c = g->configs; c->label; c++)
		PUSH_DIR_ENTRY(c->name, DT_DIR);

	for (c = g->configs; c->label; c++)
		push_config(c);

	safe_asprintf(&os_desc_path, "%s/%s/os_desc", g->path, g->name);
	PUSH_DIR(os_desc_path, 0);
}

void push_init(struct test_state *state)
{
	char **udc;
	struct test_gadget *g;
	int count = 0;

	EXPECT_OPENDIR(state->path);

	for (udc = state->udcs; *udc; udc++)
		count++;

	PUSH_DIR("/sys/class/udc", count);
	for (udc = state->udcs; *udc; udc++)
		PUSH_DIR_ENTRY(*udc, DT_REG);

	count = 0;
	for (g = state->gadgets; g->name; g++)
		count++;

	PUSH_DIR(state->path, count);
	for (g = state->gadgets; g->name; g++) {
		PUSH_DIR_ENTRY(g->name, DT_DIR);
	}

	for (g = state->gadgets; g->name; g++)
		push_gadget(g);
}

int get_gadget_attr(struct usbg_gadget_attrs *attrs, usbg_gadget_attr attr)
{
	int ret = -1;

	switch (attr) {
	case USBG_BCD_USB:
		ret = attrs->bcdUSB;
		break;
	case USBG_B_DEVICE_CLASS:
		ret = attrs->bDeviceClass;
		break;
	case USBG_B_DEVICE_SUB_CLASS:
		ret = attrs->bDeviceSubClass;
		break;
	case USBG_B_DEVICE_PROTOCOL:
		ret = attrs->bDeviceProtocol;
		break;
	case USBG_B_MAX_PACKET_SIZE_0:
		ret = attrs->bMaxPacketSize0;
		break;
	case USBG_ID_VENDOR:
		ret = attrs->idVendor;
		break;
	case USBG_ID_PRODUCT:
		ret = attrs->idProduct;
		break;
	case USBG_BCD_DEVICE:
		ret = attrs->bcdDevice;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

void pull_gadget_attribute(struct test_gadget *gadget,
			   usbg_gadget_attr attr, int value)
{
	char *path;
	char *content;

	safe_asprintf(&path, "%s/%s/%s",
		      gadget->path, gadget->name,
		      usbg_get_gadget_attr_str(attr));

	safe_asprintf(&content, "0x%x\n", value);

	EXPECT_HEX_WRITE(path, content);
}

void push_gadget_attribute(struct test_gadget *gadget,
			   usbg_gadget_attr attr, int value)
{
	char *path;
	char *content;

	safe_asprintf(&path, "%s/%s/%s",
		      gadget->path, gadget->name,
		      usbg_get_gadget_attr_str(attr));
	safe_asprintf(&content, "0x%x\n", value);

	PUSH_FILE_STR(path, content);
}

void push_gadget_attrs(struct test_gadget *gadget,
		       struct usbg_gadget_attrs *attrs)
{
	int i;

	for (i = USBG_GADGET_ATTR_MIN; i < USBG_GADGET_ATTR_MAX; i++)
		push_gadget_attribute(gadget, i, get_gadget_attr(attrs, i));
}

void pull_gadget_attrs(struct test_gadget *gadget,
		       struct usbg_gadget_attrs *attrs)
{
	int i;

	for (i = USBG_GADGET_ATTR_MIN; i < USBG_GADGET_ATTR_MAX; i++)
		pull_gadget_attribute(gadget, i, get_gadget_attr(attrs, i));
}

void init_with_state(struct test_state *in, usbg_state **out)
{
	int usbg_ret;

	push_init(in);
	usbg_ret = usbg_init(in->configfs_path, out);
	assert_int_equal(usbg_ret, USBG_SUCCESS);
}

void safe_init_with_state(void **state, struct test_state **ts, usbg_state **s)
{
	*ts = (struct test_state *)(*state);
	*state = NULL;

	init_with_state(*ts, s);
	*state = *s;
}

static int get_config_attr(struct usbg_config_attrs *attrs, config_attr attr)
{
	int ret;

	switch (attr) {
	case MAX_POWER:
		ret = attrs->bMaxPower;
		break;
	case BM_ATTRIBUTES:
		ret = attrs->bmAttributes;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

void push_config_attribute(struct test_config *config, config_attr attr,
			   int value)
{
	char *path;
	char *content;

	safe_asprintf(&path, "%s/%s/%s",
		      config->path, config->name, config_attr_names[attr]);

	switch (config_attr_format[attr]) {
	case FORMAT_HEX:
		safe_asprintf(&content, "0x%x\n", value);
		break;
	case FORMAT_DEC:
		safe_asprintf(&content, "%d\n", value);
		break;
	}

	PUSH_FILE_STR(path, content);
}


void push_config_attrs(struct test_config *config,
		       struct usbg_config_attrs *attrs)
{
	int i;

	for (i = 0; i < CONFIG_ATTR_MAX; ++i)
		push_config_attribute(config, i, get_config_attr(attrs, i));
}

void pull_config_attribute(struct test_config *config, config_attr attr,
			   int value)
{
	char *path;
	char *content;

	safe_asprintf(&path, "%s/%s/%s",
		      config->path, config->name, config_attr_names[attr]);

	switch (config_attr_format[attr]) {
	case FORMAT_HEX:
		safe_asprintf(&content, "0x%x\n", value);
		break;
	case FORMAT_DEC:
		safe_asprintf(&content, "%d\n", value);
		break;
	}

	switch (config_attr_format[attr]) {
	case FORMAT_HEX:
		EXPECT_HEX_WRITE(path, content);
		break;
	case FORMAT_DEC:
		EXPECT_WRITE_STR(path, content);
		break;
	}
}

void pull_config_attrs(struct test_config *config,
		       struct usbg_config_attrs *attrs)
{
	int i;

	for (i = 0; i < CONFIG_ATTR_MAX; ++i)
		pull_config_attribute(config, i, get_config_attr(attrs, i));
}

const char *get_gadget_str(struct usbg_gadget_strs *strs, gadget_str str)
{
	const char *ret = NULL;

	switch (str) {
	case STR_SER:
		ret = strs->serial;
		break;
	case STR_MNF:
		ret = strs->manufacturer;
		break;
	case STR_PRD:
		ret = strs->product;
		break;
	default:
		ret = NULL;
		break;
	}

	return ret;
}

static void pull_gadget_str_dir(struct test_gadget *gadget, int lang)
{
	char *dir;
	int tmp;

	safe_asprintf(&dir, "%s/%s/strings/0x%x",
			gadget->path, gadget->name, lang);

	srand(time(NULL));
	tmp = rand() % 2;

	if (tmp) {
		EXPECT_OPENDIR(dir);
	} else {
		EXPECT_OPENDIR_ERROR(dir, ENOENT);
		EXPECT_MKDIR(dir);
	}
}

static void pull_gadget_str(struct test_gadget *gadget, const char *attr_name,
			    int lang, const char *content)
{
	char *path;

	safe_asprintf(&path, "%s/%s/strings/0x%x/%s",
			gadget->path, gadget->name, lang, attr_name);
	EXPECT_WRITE_STR(path, content);
}

void pull_gadget_string(struct test_gadget *gadget, int lang,
		gadget_str str, const char *content)
{
	pull_gadget_str_dir(gadget, lang);
	pull_gadget_str(gadget, gadget_str_names[str], lang, content);
}

void pull_gadget_strs(struct test_gadget *gadget, int lang,
		      struct usbg_gadget_strs *strs)
{
	int i;

	pull_gadget_str_dir(gadget, lang);
	for (i = 0; i < GADGET_STR_MAX; i++)
		pull_gadget_str(gadget, gadget_str_names[i],
				lang, get_gadget_str(strs, i));
}

static void push_gadget_str_dir(struct test_gadget *gadget, int lang)
{
	char *dir;

	safe_asprintf(&dir, "%s/%s/strings/0x%x",
			gadget->path, gadget->name, lang);

	EXPECT_OPENDIR(dir);
}

static void push_gadget_str(struct test_gadget *gadget, const char *attr_name,
			    int lang, const char *content)
{
	char *path;

	safe_asprintf(&path, "%s/%s/strings/0x%x/%s",
			gadget->path, gadget->name, lang, attr_name);
	PUSH_FILE_STR(path, content);
}

void push_gadget_strs(struct test_gadget *gadget, int lang,
		      struct usbg_gadget_strs *strs)
{
	int i;

	push_gadget_str_dir(gadget, lang);
	for (i = 0; i < GADGET_STR_MAX; i++)
		push_gadget_str(gadget, gadget_str_names[i],
				lang, get_gadget_str(strs, i));
}

void pull_config_string(struct test_config *config, int lang, const char *str)
{
	char *path;
	int tmp;


	safe_asprintf(&path, "%s/%s/strings/0x%x",
			config->path, config->name, lang);

	srand(time(NULL));
	tmp = rand() % 2;

	if (tmp) {
		EXPECT_OPENDIR(path);
	} else {
		EXPECT_OPENDIR_ERROR(path, ENOENT);
		EXPECT_MKDIR(path);
	}

	safe_asprintf(&path, "%s/configuration", path);

	EXPECT_WRITE_STR(path, str);
}

void pull_config_strs(struct test_config *config, int lang,
		      struct usbg_config_strs *strs)
{
	pull_config_string(config, lang, strs->configuration);
}

void push_config_string(struct test_config *config, int lang, const char *str)
{
	char *path;

	safe_asprintf(&path, "%s/%s/strings/0x%x",
			config->path, config->name, lang);

	EXPECT_OPENDIR(path);

	safe_asprintf(&path, "%s/configuration", path);

	PUSH_FILE_STR(path, str);
}

void push_config_strs(struct test_config *config, int lang,
		      struct usbg_config_strs *strs)
{
	push_config_string(config, lang, strs->configuration);
}

void assert_config_attrs_equal(struct usbg_config_attrs *actual,
			       struct usbg_config_attrs *expected)
{
	assert_int_equal(actual->bmAttributes, expected->bmAttributes);
	assert_int_equal(actual->bMaxPower, expected->bMaxPower);
}

void pull_create_config(struct test_config *tc)
{
	char *path;

	safe_asprintf(&path, "%s/%s", tc->path, tc->name);
	EXPECT_MKDIR(path);

	if (tc->attrs)
		pull_config_attrs(tc, tc->attrs);
	if (tc->strs)
		pull_config_strs(tc, LANG_US_ENG, tc->strs);
}

#define ETHER_ADDR_STR_LEN 19

static void push_serial_attrs(struct test_function *func,
			      int *port_num)
{
	char *path;
	char *content;

	safe_asprintf(&path, "%s/%s/port_num", func->path, func->name);
	safe_asprintf(&content, "%d\n", *port_num);
	PUSH_FILE_STR(path, content);
}

static void push_net_attrs(struct test_function *func,
			   struct usbg_f_net_attrs *attrs)
{
	char *path;
	char *content;

	safe_asprintf(&path, "%s/%s/dev_addr", func->path, func->name);

	content = safe_malloc(ETHER_ADDR_STR_LEN * sizeof(char));
	ether_ntoa_r(&attrs->dev_addr, content);

	PUSH_FILE_STR(path, content);

	path = safe_malloc(USBG_MAX_PATH_LENGTH * sizeof(char));
	sprintf(path, "%s/%s/host_addr",
			func->path, func->name);

	content = safe_malloc(ETHER_ADDR_STR_LEN * sizeof(char));
	ether_ntoa_r(&attrs->host_addr, content);

	PUSH_FILE_STR(path, content);

	safe_asprintf(&path, "%s/%s/ifname", func->path, func->name);
	safe_asprintf(&content, "%s\n", attrs->ifname);
	PUSH_FILE_STR(path, content);

	safe_asprintf(&path, "%s/%s/qmult", func->path, func->name);
	safe_asprintf(&content, "%d\n", attrs->qmult);
	PUSH_FILE_STR(path, content);
}

static void push_phonet_attrs(struct test_function *func,
			      char **ifname)
{
	char *path;
	char *content;

	safe_asprintf(&path, "%s/%s/ifname", func->path, func->name);
	safe_asprintf(&content, "%s\n", *ifname);
	PUSH_FILE_STR(path, content);
}

void push_function_attrs(struct test_function *func, void *function_attrs)
{
	switch (func->type) {
	case USBG_F_ACM:
	case USBG_F_OBEX:
	case USBG_F_SERIAL:
		push_serial_attrs(func, function_attrs);
		break;
	case USBG_F_ECM:
	case USBG_F_SUBSET:
	case USBG_F_NCM:
	case USBG_F_EEM:
	case USBG_F_RNDIS:
		push_net_attrs(func, function_attrs);
		break;
	case USBG_F_PHONET:
		push_phonet_attrs(func, function_attrs);
		break;
	case USBG_F_FFS:
		// ffs does not exist in filesystem
	default:
		break;
	}
}

static void pull_function_net_attrs(struct test_function *func,
				    struct usbg_f_net_attrs *attrs)
{
	char *path;
	char *content;

	safe_asprintf(&path, "%s/%s/dev_addr", func->path, func->name);

	content = safe_malloc(ETHER_ADDR_STR_LEN * sizeof(char));
	usbg_ether_ntoa_r(&attrs->dev_addr, content);

	EXPECT_WRITE_STR(path, content);

	safe_asprintf(&path, "%s/%s/host_addr", func->path, func->name);

	content = safe_malloc(ETHER_ADDR_STR_LEN * sizeof(char));
	usbg_ether_ntoa_r(&attrs->host_addr, content);

	EXPECT_WRITE_STR(path, content);

	safe_asprintf(&path, "%s/%s/qmult", func->path, func->name);
	safe_asprintf(&content, "%d\n", attrs->qmult);
	EXPECT_WRITE_STR(path, content);
}

void pull_function_attrs(struct test_function *func, void *attrs)
{
	switch (func->type) {
	case USBG_F_ECM:
	case USBG_F_SUBSET:
	case USBG_F_NCM:
	case USBG_F_EEM:
	case USBG_F_RNDIS:
		pull_function_net_attrs(func, attrs);
		break;
	default:
		break;
	}
}

void pull_create_function(struct test_function *tf)
{
	char *path;
	int tmp;

	tmp = asprintf(&path, "%s/%s", tf->path, tf->name);
	if (tmp < 0)
		fail();
	free_later(path);

	EXPECT_MKDIR(path);
	if (tf->attrs)
		pull_function_attrs(tf, tf->attrs);
}

void assert_func_equal(usbg_function *f, struct test_function *expected)
{
	assert_string_equal(f->instance, expected->instance);
	assert_int_equal(f->type, expected->type);
	assert_path_equal(f->path, expected->path);
}

void assert_binding_equal(usbg_binding *b, struct test_binding *expected)
{
	assert_string_equal(b->name, expected->name);
	assert_func_equal(b->target, expected->target);
}

void assert_config_equal(usbg_config *c, struct test_config *expected)
{
	int i = 0;
	usbg_binding *b;

	assert_int_equal(c->id, expected->id);
	assert_string_equal(c->label, expected->label);
	assert_path_equal(c->path, expected->path);
	usbg_for_each_binding(b, c)
		assert_binding_equal(b, &expected->bindings[i++]);
}

void assert_gadget_equal(usbg_gadget *g, struct test_gadget *expected)
{
	usbg_config *c;
	usbg_function *f;
	int i;

	assert_string_equal(g->name, expected->name);
	assert_path_equal(g->path, expected->path);

	i = 0;
	usbg_for_each_function(f, g)
		assert_func_equal(f, &expected->functions[i++]);

	i = 0;
	usbg_for_each_config(c, g)
		assert_config_equal(c, &expected->configs[i++]);
}

void assert_state_equal(usbg_state *s, struct test_state *expected)
{
	usbg_gadget *g;
	int i = 0;

	assert_path_equal(s->path, expected->path);
	assert_path_equal(s->configfs_path, expected->configfs_path);

	usbg_for_each_gadget(g, s)
		assert_gadget_equal(g, &expected->gadgets[i++]);
}

#define SIGNUM(x) (((x) > 0) - ((x) < 0))

int path_cmp(const char *actual, const char *expected)
{
	const char *a = actual;
	const char *b = expected;

	while (*a != '\0' && *b != '\0') {
		if (*a != *b)
			break;
		do
			++a;
		while (*a == '/');

		do
			++b;
		while (*b == '/');
	}

	return SIGNUM(*a - *b);
}

int path_equal_display_error(const LargestIntegralType actual,
			     const LargestIntegralType expected)
{
	if (path_cmp((const char *)actual, (const char *)expected) == 0) {
		return 1;
	}

	fprintf(stderr, "%s != %s\n",
		(const char *)actual, (const char *)expected);
	return 0;
}

void assert_path_equal(const char *actual, const char *expected)
{
	if (path_equal_display_error(
		    cast_to_largest_integral_type(actual),
		    cast_to_largest_integral_type(expected)) == 0)
		fail();
}

int hex_str_cmp(const char *actual, const char *expected)
{
	int a, b;

	sscanf(actual, "%x", &a);
	sscanf(expected, "%x", &b);

	return SIGNUM(a - b);
}

int hex_str_equal_display_error(const LargestIntegralType actual,
				const LargestIntegralType expected)
{
	if (hex_str_cmp((const char *)actual, (const char *)expected) == 0) {
		return 1;
	}

	fprintf(stderr, "%s != %s\n",
		(const char *)actual, (const char *)expected);
	return 0;
}

void assert_gadget_attrs_equal(struct usbg_gadget_attrs *actual,
		struct usbg_gadget_attrs *expected)
{
	int i;

	for (i = USBG_GADGET_ATTR_MIN; i < USBG_GADGET_ATTR_MAX; i++)
		assert_int_equal(get_gadget_attr(actual, i),
				 get_gadget_attr(expected, i));
}

void assert_gadget_strs_equal(struct usbg_gadget_strs *actual,
			      struct usbg_gadget_strs *expected)
{
	int i;
	for (i = 0; i < GADGET_STR_MAX; i++)
		assert_string_equal(get_gadget_str(actual, i),
				    get_gadget_str(expected, i));
}

void assert_f_serial_attrs_equal(int *actual, int *expected)
{
	assert_int_equal(*actual, *expected);
}

static void assert_ether_addrs_equal(const struct ether_addr *ea1,
		const struct ether_addr *ea2)
{
	assert_memory_equal(ea1->ether_addr_octet, ea2->ether_addr_octet,
			    ETHER_ADDR_LEN);
}

void assert_f_net_attrs_equal(struct usbg_f_net_attrs *actual,
			      struct usbg_f_net_attrs *expected)
{
	assert_ether_addrs_equal(&actual->dev_addr, &expected->dev_addr);
	assert_ether_addrs_equal(&actual->host_addr, &expected->host_addr);
	assert_string_equal(actual->ifname, expected->ifname);
	assert_int_equal(actual->qmult, expected->qmult);
}

void assert_f_phonet_attrs_equal(char  **actual, char **expected)
{
	assert_string_equal(*actual, *expected);
}

void assert_f_ffs_attrs_equal(char **actual, char **expected)
{
	assert_string_equal(*actual, *expected);
}

void assert_function_attrs_equal(void *actual, void *expected,
				 usbg_function_type type)
{
	switch (type) {
	case USBG_F_ACM:
	case USBG_F_OBEX:
	case USBG_F_SERIAL:
		assert_f_serial_attrs_equal(actual, expected);
		break;
	case USBG_F_ECM:
	case USBG_F_SUBSET:
	case USBG_F_NCM:
	case USBG_F_EEM:
	case USBG_F_RNDIS:
		assert_f_net_attrs_equal(actual, expected);
		break;
	case USBG_F_PHONET:
		assert_f_phonet_attrs_equal(actual, expected);
		break;
	case USBG_F_FFS:
		assert_f_ffs_attrs_equal(actual, expected);
		break;
	default:
		fail();
	}
}


void for_each_test_function(struct test_state *ts, usbg_state *s,
			    FunctionTest fun)
{
	struct test_gadget *tg;
	struct test_function *tf;
	usbg_gadget *g = NULL;
	usbg_function *f = NULL;

	for (tg = ts->gadgets; tg->name; ++tg) {
		g = usbg_get_gadget(s, tg->name);
		assert_non_null(g);
		for (tf = tg->functions; tf->instance; ++tf) {
			f = usbg_get_function(g, tf->type, tf->instance);
			fun(f, tf);
		}
	}
}

void for_each_test_config(struct test_state *ts, usbg_state *s, ConfigTest fun)
{
	usbg_gadget *g = NULL;
	usbg_config *c = NULL;
	struct test_gadget *tg;
	struct test_config *tc;

	for (tg = ts->gadgets; tg->name; tg++) {
		g = usbg_get_gadget(s, tg->name);
		assert_non_null(g);
		for (tc = tg->configs; tc->label; tc++) {
			c = usbg_get_config(g, tc->id, tc->label);
			fun(c, tc);
		}
	}
}

void for_each_binding(struct test_state *ts, usbg_state *s, BindingTestFunc fun)
{
	struct test_gadget *tg;
	struct test_config *tc;
	struct test_binding *tb;
	usbg_gadget *g = NULL;
	usbg_config *c = NULL;
	usbg_binding *b = NULL;

	for (tg = ts->gadgets; tg->name; tg++) {
		g = usbg_get_gadget(s, tg->name);
		assert_non_null(g);
		for (tc = tg->configs; tc->label; tc++) {
			c = usbg_get_config(g, tc->id, tc->label);
			assert_non_null(c);

			b = usbg_get_first_binding(c);
			for (tb = tc->bindings; tb->name; ++tb) {
				assert_non_null(b);
				fun(tb, b);
				b = usbg_get_next_binding(b);
			}
		}
	}
}

void for_each_test_gadget(struct test_state *ts, usbg_state *s,
			  GadgetTestFunc fun)
{
	struct test_gadget *tg;
	usbg_gadget *g = NULL;

	for (tg = ts->gadgets; tg->name; ++tg) {
		g = usbg_get_gadget(s, tg->name);
		assert_non_null(g);
		fun(g, tg);
	}
}
