#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

typedef enum
{
    POWERSUPPLY = 'A',
    RESISTOR = 'B',
    TRANSISTOR = 'C'
} ComponentType;

typedef struct
{
    int id;
    double voltage;
} Powersupply;

typedef struct
{
    int id;
    double resistance;
    enum
    {
        CALC_VOLTAGE,
        CALC_CURRENT
    };
} Resistor;

typedef struct
{
    int id;
    bool type;
    bool input_type;
} Transistor;

typedef union
{
    Powersupply powersupply;
    Resistor resistor;
    Transistor transistor;
} ComponentData;

typedef struct
{
    ComponentData data;
    ComponentType type;
    Component **anode_connections;
    Component **cathode_connections;
} Component;

typedef struct
{
    Component *data;
    size_t size;
    size_t capacity;
} ComponentArray;

void addComponent(ComponentArray *arr, Component value)
{
    if (arr->size == arr->capacity)
    {
        size_t new_cappacity = arr->capacity * 2;
        if (new_cappacity == 0)
        {
            new_cappacity = 1;
        }

        Component *new_data = (Component *)realloc(arr->data, new_cappacity * sizeof(Component));
        if (new_data == NULL)
        {
            perror("Failed to allocated memory for Dynamic array");
            exit(EXIT_FAILURE);
        }
        arr->data = new_data;
        arr->capacity = new_cappacity;
    }
    arr->data[arr->size] = value;
    arr->size++;
}

Component getComponent(ComponentArray *arr, size_t index)
{
    return arr->data[index];
}

void freeComponentArray(ComponentArray *arr)
{
    free(arr->data);
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

double resistor(double resistance, double input, bool type)
{
    if (type)
        return input * resistance;
    else
        return input / resistance;
}

double transistor(double input_control_current_or_voltage, double base_current, bool is_NPN, bool return_collector_current)
{
    double transistor_beta = 100.0;

    if (is_NPN)
    {
        if (input_control_current_or_voltage > 0.1 && input_control_current_or_voltage < 0.8)
        {
            if (base_current > 0.00001 && base_current < 0.01)
            {
                if (return_collector_current)
                    return transistor_beta * base_current;
                else
                    return input_control_current_or_voltage + base_current;
            }
        }
    }
    else
    {
        if (input_control_current_or_voltage > 0.1 && input_control_current_or_voltage < 0.8)
        {
            if (base_current > 0 && base_current < 0.01)
            {
                if (return_collector_current)
                    return transistor_beta * base_current;
                else
                    return input_control_current_or_voltage + base_current;
            }
        }
    }
    return 0.0;
}

int led_bulb(double current)
{
    if (current < 0.01)
        return 0;
    else if (current > 0.02)
    {
        printf("LED burn, current limit of 20mA exceeded: %f\n", current);
        return 0;
    }
    else
        return 1;
}

int main()
{
    ComponentArray component_array;
    component_array.size = 0;
    component_array.capacity = component_array.size;
    component_array.data = (Component *)malloc(component_array.capacity * sizeof(Component));
    if (component_array.data == NULL)
    {
        perror("Failed to allocated memory for Dynamic array");
        exit(EXIT_FAILURE);
    }

    printf("Welcome to Electronic CAD\n");
    bool quit = false;
    double voltage = 0.0;
    while (!quit)
    {
        char item_type;
        printf("Enter what to do\n");
        printf("A: Power Supply\tB: Resistor\tC: Transistor\tD:LED\n");
        scanf("%c", &item_type);

        Component new_component;

        switch (item_type)
        {
        case 'A':
            printf("Enter Voltage:\t");
            scanf("%f", &voltage);
            new_component.data.powersupply.voltage = voltage;
            new_component.data.powersupply.id = component_array.size;
            new_component.type = POWERSUPPLY;
            addComponent(&component_array, new_component);
            break;
        case 'B':
            double resistance = 0.0;
            printf("Enter resistance:\t");
            scanf("%f", &resistance);
            new_component.data.resistor.resistance = resistance;
            new_component.data.resistor.id = component_array.size;
            new_component.type = RESISTOR;
            addComponent(&component_array, new_component);
            break;

        case 'C':
            bool type;
            bool output_type;
            double base_current;
            double input_current;

            printf("Enter Transistor type (NPN or PNP) 0 or 1:\t");
            scanf("%d", &type);
            new_component.data.transistor.type = type;

            printf("Enter the format of output (Voltage or Current) 0 or 1:\t");
            scanf("%d", &output_type);
            new_component.data.transistor.input_type = output_type;

            new_component.type = TRANSISTOR;

            addComponent(&component_array, new_component);
            break;
        default:
            break;
        }
    }
}