/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <zephyr/kernel.h>
#include <drivers/gpio.h>
#include <zephyr/device.h>
#include <string.h>

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

K_THREAD_STACK_DEFINE(my_stack_area, STACKSIZE);
K_THREAD_STACK_DEFINE(my_stack_area_2, STACKSIZE);
struct k_thread blink2;
struct k_thread print_fifo;

#define LED1_NODE 	DT_ALIAS(led1)
#define LED2_NODE 	DT_ALIAS(led2)
#define SW1_NODE	DT_ALIAS(sw1)

static const struct gpio_dt_spec led  = GPIO_DT_SPEC_GET(LED1_NODE,gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE,gpios);
static const struct gpio_dt_spec sw   = GPIO_DT_SPEC_GET(SW1_NODE,gpios);

static struct k_delayed_work sw_work;

static struct gpio_callback gpio_cb_data;


struct printk_data_t {
	void *fifo_reserved; /* 1st word reserved for use by fifo */
	uint32_t led;
	uint32_t cnt;
};
K_FIFO_DEFINE(fifo_q);
void blink_thread(void);
void print_fifo_thread(void);



void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pin)
{
	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	k_delayed_work_submit(&sw_work,K_SECONDS(2));

}

void sw_work_fn(struct k_work *item)
{
	int ret;
	int val = gpio_pin_get_dt(&sw);
	static uint32_t cnt = 0;
	
	if (val > 0)
	{
		printk("SW Still pressed\n");
		ret = gpio_pin_toggle_dt(&led);
		printk("pin toggle\n");
		if (ret < 0)
		{
			return;
		}
		struct printk_data_t tx_data = { .led = 1, .cnt = cnt };
		size_t size = sizeof(struct printk_data_t);
		char *mem_ptr = k_malloc(size);
		memcpy(mem_ptr,&tx_data,size);
		k_fifo_put(&fifo_q,mem_ptr);
		k_msleep(1000);
		cnt++;
	}
}

void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

	int ret;
	if(!device_is_ready(sw.port))
	{
		return;
	}
	if(!device_is_ready(led.port))
	{
		return;
	}
	printk("ready dev\n");
	ret = gpio_pin_configure_dt(&led,GPIO_OUTPUT_ACTIVE);
	if (ret != 0)
	{
		return;
	}
	ret = gpio_pin_configure_dt(&sw,GPIO_INPUT);
	if (ret != 0)
	{
		return;
	}
	ret = gpio_pin_interrupt_configure_dt(&sw,GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, sw.port->name, sw.pin);
		return;
	}
	printk("cfg done \n");

	gpio_init_callback(&gpio_cb_data,button_pressed,BIT(sw.pin));
	gpio_add_callback(sw.port,&gpio_cb_data);
	k_delayed_work_init(&sw_work,sw_work_fn);

	///thread create
	k_tid_t blink_tid = k_thread_create(&blink2, my_stack_area,
                                 K_THREAD_STACK_SIZEOF(my_stack_area),
                                 (k_thread_entry_t)blink_thread,
                                 NULL, NULL, NULL,
                                 PRIORITY, 0, K_NO_WAIT);
	k_tid_t print_fifo_tid = k_thread_create(&print_fifo, my_stack_area_2,
                                 K_THREAD_STACK_SIZEOF(my_stack_area_2),
                                 (k_thread_entry_t)print_fifo_thread,
                                 NULL, NULL, NULL,
                                 PRIORITY, 0, K_NO_WAIT);
	k_thread_start(print_fifo_tid);	
	k_thread_start(blink_tid);					 
	while (1)
	{
		/*int val = gpio_pin_get_dt(&sw);
		if (val > 0)
		{
			ret = gpio_pin_toggle_dt(&led);
			printk("pin toggle\n");
			if (ret < 0)
			{
				return;
			}
		}*/
		k_msleep(1000);
	}
}



void blink_thread()
{
	uint32_t cnt = 0;
	int ret = 0;
	printk("blink thread start\n");
	if(!device_is_ready(led2.port))
	{
		return;
	}
	ret = gpio_pin_configure_dt(&led2,GPIO_OUTPUT_ACTIVE);
	if (ret != 0)
	{
		return;
	}
	while (1)
	{
		ret = gpio_pin_toggle_dt(&led2);
		if (ret < 0)
		{
			return;
		}
		struct printk_data_t tx_data = { .led = 2, .cnt = cnt };
		size_t size = sizeof(struct printk_data_t);
		char *mem_ptr = k_malloc(size);
		memcpy(mem_ptr,&tx_data,size);
		k_fifo_put(&fifo_q,mem_ptr);
		k_msleep(1000);
		cnt++;

	}
	
}

void print_fifo_thread()
{
	printk("print_fifo_thread start\n");
	while (1)
	{
		struct printk_data_t *rx_data = k_fifo_get(&fifo_q,K_FOREVER);
		printk("Toggled led%d; counter=%d\n",
		       rx_data->led, rx_data->cnt);
		k_free(rx_data);
		k_msleep(800);
	}
	
}

