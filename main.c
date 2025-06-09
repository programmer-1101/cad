#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "cJSON/cJSON.h"

typedef enum
{
    POWERSUPPLY = 'A',
    RESISTOR = 'B',
    TRANSISTOR = 'C'
} ComponentType;

typedef struct
{
    int anode;
    int cathode;
    int id;
    double voltage;
    int pin1;
} Powersupply;

typedef struct
{
    int id;
    double resistance;
    enum
    {
        CALC_VOLTAGE,
        CALC_CURRENT
    } otype;
    double input;
    double output;
    int pin1;
    int pin2;
} Resistor;

typedef struct
{
    int id;
    bool type;
    bool input_type;
    double input;
    double base;
    double output;
    int pin1;
    int pin2;
    int pin3;
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
        size_t new_capacity = (arr->capacity == 0) ? 1 : arr->capacity * 2;
        Component *new_data = realloc(arr->data, new_capacity * sizeof(Component));
        if (!new_data)
        {
            perror("Failed to allocate memory");
            exit(EXIT_FAILURE);
        }
        arr->data = new_data;
        arr->capacity = new_capacity;
    }
    arr->data[arr->size++] = value;
}

Component getComponent(ComponentArray *arr, size_t index)
{
    return arr->data[index];
}

void freeComponentArray(ComponentArray *arr)
{
    free(arr->data);
    arr->data = NULL;
    arr->size = arr->capacity = 0;
}

double resistor_calc(double resistance, double input, int otype)
{
    return otype == CALC_VOLTAGE ? (input * resistance) : (input / resistance);
}

double transistor_calc(double input, double base, bool is_NPN, bool input_type)
{
    double beta = 100.0;
    if (is_NPN && base > 0.7)
        return input_type ? beta * base : input - 0.7;
    else if (!is_NPN && base < -0.7)
        return input_type ? beta * -base : input + 0.7;
    return 0.0;
}

int led_bulb(double current)
{
    if (current < 0.01)
        return 0;
    else if (current > 0.02)
    {
        printf("LED burned! Current exceeded 20mA: %.3fA\n", current);
        return 0;
    }
    return 1;
}

void saveCircuit(char *file_name, ComponentArray *array)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *components = cJSON_CreateArray();
    if (!root || !components)
    {
        fprintf(stderr, "JSON allocation failed.\n");
        return;
    }

    for (size_t i = 0; i < array->size; i++)
    {
        Component *comp = &array->data[i];
        cJSON *obj = cJSON_CreateObject();

        if (!obj)
            continue;

        cJSON_AddNumberToObject(obj, "id", i);

        switch (comp->type)
        {
        case POWERSUPPLY:
            cJSON_AddStringToObject(obj, "type", "PowerSupply");
            cJSON_AddNumberToObject(obj, "voltage", comp->data.powersupply.voltage);
            cJSON_AddNumberToObject(obj, "pin1", comp->data.powersupply.pin1);
            break;
        case RESISTOR:
            cJSON_AddStringToObject(obj, "type", "Resistor");
            cJSON_AddNumberToObject(obj, "resistance", comp->data.resistor.resistance);
            cJSON_AddStringToObject(obj, "output_type", comp->data.resistor.otype == CALC_VOLTAGE ? "Voltage" : "Current");
            cJSON_AddNumberToObject(obj, "pin1", comp->data.resistor.pin1);
            cJSON_AddNumberToObject(obj, "pin2", comp->data.resistor.pin2);
            break;
        case TRANSISTOR:
            cJSON_AddStringToObject(obj, "type", "Transistor");
            cJSON_AddStringToObject(obj, "transistor_type", comp->data.transistor.type ? "PNP" : "NPN");
            cJSON_AddStringToObject(obj, "input_output_format", comp->data.transistor.input_type ? "Current" : "Voltage");
            cJSON_AddNumberToObject(obj, "pin1", comp->data.transistor.pin1);
            cJSON_AddNumberToObject(obj, "pin2", comp->data.transistor.pin2);
            cJSON_AddNumberToObject(obj, "pin3", comp->data.transistor.pin3);
            break;
        }

        cJSON_AddItemToArray(components, obj);
    }

    cJSON_AddItemToObject(root, "components", components);

    char *json_str = cJSON_Print(root);
    if (!json_str)
    {
        fprintf(stderr, "Failed to print JSON.\n");
        cJSON_Delete(root);
        return;
    }

    FILE *fp = fopen(file_name, "w");
    if (!fp)
    {
        perror("File write failed");
        free(json_str);
        cJSON_Delete(root);
        return;
    }

    fputs(json_str, fp);
    fclose(fp);
    free(json_str);
    cJSON_Delete(root);
    printf("Circuit saved to '%s'\n", file_name);
}

void loadCircuit(const char *file_name, ComponentArray *array)
{
    FILE *fp = fopen(file_name, "r");
    if (!fp)
    {
        perror("Error opening file");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    char *buffer = malloc(len + 1);
    fread(buffer, 1, len, fp);
    buffer[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    if (!root)
    {
        fprintf(stderr, "JSON Parse Error: %s\n", cJSON_GetErrorPtr());
        return;
    }

    cJSON *components = cJSON_GetObjectItem(root, "components");
    if (!cJSON_IsArray(components))
    {
        fprintf(stderr, "Invalid JSON format: components not array\n");
        cJSON_Delete(root);
        return;
    }

    freeComponentArray(array);
    array->capacity = cJSON_GetArraySize(components) + 1;
    array->data = malloc(array->capacity * sizeof(Component));
    array->size = 0;

    cJSON *item;
    cJSON_ArrayForEach(item, components)
    {
        Component new_comp = {0};
        cJSON *id = cJSON_GetObjectItem(item, "id");
        cJSON *type = cJSON_GetObjectItem(item, "type");

        if (!cJSON_IsNumber(id) || !cJSON_IsString(type))
            continue;

        if (strcmp(type->valuestring, "PowerSupply") == 0)
        {
            new_comp.type = POWERSUPPLY;
            new_comp.data.powersupply.id = id->valueint;
            new_comp.data.powersupply.voltage = cJSON_GetObjectItem(item, "voltage")->valuedouble;
            new_comp.data.powersupply.pin1 = cJSON_GetObjectItem(item, "pin1")->valueint;
        }
        else if (strcmp(type->valuestring, "Resistor") == 0)
        {
            new_comp.type = RESISTOR;
            new_comp.data.resistor.id = id->valueint;
            new_comp.data.resistor.resistance = cJSON_GetObjectItem(item, "resistance")->valuedouble;
            cJSON *out = cJSON_GetObjectItem(item, "output_type");
            new_comp.data.resistor.otype = (out && strcmp(out->valuestring, "Current") == 0) ? CALC_CURRENT : CALC_VOLTAGE;
            new_comp.data.resistor.pin1 = cJSON_GetObjectItem(item, "pin1")->valueint;
            new_comp.data.resistor.pin2 = cJSON_GetObjectItem(item, "pin2")->valueint;
        }
        else if (strcmp(type->valuestring, "Transistor") == 0)
        {
            new_comp.type = TRANSISTOR;
            new_comp.data.transistor.id = id->valueint;
            cJSON *ttype = cJSON_GetObjectItem(item, "transistor_type");
            new_comp.data.transistor.type = (ttype && strcmp(ttype->valuestring, "PNP") == 0);
            cJSON *io = cJSON_GetObjectItem(item, "input_output_format");
            new_comp.data.transistor.input_type = (io && strcmp(io->valuestring, "Current") == 0);
            new_comp.data.transistor.pin1 = cJSON_GetObjectItem(item, "pin1")->valueint;
            new_comp.data.transistor.pin2 = cJSON_GetObjectItem(item, "pin2")->valueint;
            new_comp.data.transistor.pin3 = cJSON_GetObjectItem(item, "pin3")->valueint;
        }

        addComponent(array, new_comp);
    }

    cJSON_Delete(root);
    printf("Circuit loaded from '%s'\n", file_name);
}

void list_components(ComponentArray *component_array)
{
    for (size_t i = 0; i < component_array->size; i++)
    {
        Component *c = &component_array->data[i];
        printf("ID %zu - ", i);

        switch(c->type)
        {
        case POWERSUPPLY:
            printf("PowerSupply: %.2fV", c->data.powersupply.voltage);
            if (c->data.powersupply.pin1 != -1)
                printf(" (Connected to ID: %d)", c->data.powersupply.pin1);
            break;
        case RESISTOR:
            printf("Resistor: %.2f Ohms, Output: %.2f %s",
                           c->data.resistor.resistance,
                           c->data.resistor.output,
                           c->data.resistor.otype == CALC_VOLTAGE ? "V" : "A");
            if (c->data.resistor.pin1 != -1 || c->data.resistor.pin2 != -1)
                printf(" (Pins: %d, %d)", c->data.resistor.pin1, c->data.resistor.pin2);
            break;
        case TRANSISTOR:
            printf("Transistor: %s, Output: %.2f",
                           c->data.transistor.type ? "PNP" : "NPN",
                           c->data.transistor.output);
            if (c->data.transistor.pin1 != -1 || c->data.transistor.pin2 != -1 || c->data.transistor.pin3 != -1)
                printf(" (Pins: %d, %d, %d)",
                               c->data.transistor.pin1,
                               c->data.transistor.pin2,
                               c->data.transistor.pin3);
            break;
        }
        printf("\n");
    }
}

int main()
{
    ComponentArray component_array = {0};
    char input_buffer[256];
    bool quit = false;

    printf("Welcome to Electronic CAD\n");

    while (!quit)
    {
        printf("\nA: Power Supply\nB: Resistor\nC: Transistor\nD: LED\nL: List\nS: Save\nF: Load\nQ: Quit\n> ");

        if (!fgets(input_buffer, sizeof(input_buffer), stdin))
            break;

        char cmd = toupper(input_buffer[0]);
        Component new_component = {0};

        switch (cmd)
        {
            case 'A':
                printf("Enter voltage: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin) && sscanf(input_buffer, "%lf", &new_component.data.powersupply.voltage) == 1)
                {
                    new_component.data.powersupply.id = component_array.size;
                    new_component.data.powersupply.pin1 = -1;
                    new_component.type = POWERSUPPLY;
                    addComponent(&component_array, new_component);
                    printf("Power Supply added.\n");
                }
                break;

            case 'B':
                printf("Enter resistance (Ohms): ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin) && sscanf(input_buffer, "%lf", &new_component.data.resistor.resistance) == 1)
                {
                    new_component.data.resistor.id = component_array.size;
                    new_component.type = RESISTOR;
                    int otype = 0;
                    int iid = -1;

                    printf("0: Calc Voltage, 1: Calc Current: ");
                    if (fgets(input_buffer, sizeof(input_buffer), stdin))
                        sscanf(input_buffer, "%d", &otype);
                    new_component.data.resistor.otype = (otype == 1) ? CALC_CURRENT : CALC_VOLTAGE;
                    
                    list_components(&component_array);
                    printf("Enter Component ID to connect to: ");
                    if (fgets(input_buffer, sizeof(input_buffer), stdin))
                        sscanf(input_buffer, "%d", &iid);
                    
                    if (iid >= 0 && iid < component_array.size)
                    {
                        Component *c = &component_array.data[iid];
                        new_component.data.resistor.pin1 = iid;
                        
                        double input_value = 0.0;
                        switch(c->type)
                        {
                        case POWERSUPPLY:
                            input_value = c->data.powersupply.voltage;
                            break;
                        case TRANSISTOR:
                            input_value = c->data.transistor.output;
                            break;
                        case RESISTOR:
                            input_value = c->data.resistor.output;
                            break;
                        }
                        
                        new_component.data.resistor.input = input_value;
                        new_component.data.resistor.output = resistor_calc(
                            new_component.data.resistor.resistance,
                            input_value,
                            new_component.data.resistor.otype
                        );
                        
                        switch(c->type)
                        {
                        case POWERSUPPLY:
                            c->data.powersupply.pin1 = component_array.size;
                            break;
                        case TRANSISTOR:
                            if (c->data.transistor.pin1 == -1) c->data.transistor.pin1 = component_array.size;
                            else if (c->data.transistor.pin2 == -1) c->data.transistor.pin2 = component_array.size;
                            else if (c->data.transistor.pin3 == -1) c->data.transistor.pin3 = component_array.size;
                            break;
                        case RESISTOR:
                            if (c->data.resistor.pin1 == -1) c->data.resistor.pin1 = component_array.size;
                            else if (c->data.resistor.pin2 == -1) c->data.resistor.pin2 = component_array.size;
                            break;
                        }
                    }
                    else
                    {
                        printf("Invalid component ID, using default input value\n");
                        new_component.data.resistor.input = 5.0;
                        new_component.data.resistor.output = resistor_calc(
                            new_component.data.resistor.resistance,
                            5.0,
                            new_component.data.resistor.otype
                        );
                        new_component.data.resistor.pin1 = -1;
                    }
                    
                    new_component.data.resistor.pin2 = -1;
                    addComponent(&component_array, new_component);
                    printf("Resistor added.\n");
                }
                break;

            case 'C':
                int ttype = 0, io = 0, iid = -1, bid = -1;
                printf("0: NPN, 1: PNP: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin))
                    sscanf(input_buffer, "%d", &ttype);
                printf("0: Voltage input, 1: Current input: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin))
                    sscanf(input_buffer, "%d", &io);

                list_components(&component_array);
                printf("Enter input component ID: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin))
                    sscanf(input_buffer, "%d", &iid);
                
                printf("Enter base component ID: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin))
                    sscanf(input_buffer, "%d", &bid);

                new_component.data.transistor.id = component_array.size;
                new_component.data.transistor.type = ttype;
                new_component.data.transistor.input_type = io;
                new_component.data.transistor.pin1 = iid;
                new_component.data.transistor.pin2 = bid;
                new_component.data.transistor.pin3 = -1;
                new_component.type = TRANSISTOR;

                double input_value = 0.0;
                double base_value = 0.0;
                
                if (iid >= 0 && iid < component_array.size)
                {
                    Component *c = &component_array.data[iid];
                    switch(c->type)
                    {
                    case POWERSUPPLY:
                        input_value = c->data.powersupply.voltage;
                        break;
                    case TRANSISTOR:
                        input_value = c->data.transistor.output;
                        break;
                    case RESISTOR:
                        input_value = c->data.resistor.output;
                        break;
                    }
                }
                
                if (bid >= 0 && bid < component_array.size)
                {
                    Component *c = &component_array.data[bid];
                    switch(c->type)
                    {
                    case POWERSUPPLY:
                        base_value = c->data.powersupply.voltage;
                        break;
                    case TRANSISTOR:
                        base_value = c->data.transistor.output;
                        break;
                    case RESISTOR:
                        base_value = c->data.resistor.output;
                        break;
                    }
                }
                
                new_component.data.transistor.input = input_value;
                new_component.data.transistor.base = base_value;
                new_component.data.transistor.output = transistor_calc(
                    input_value,
                    base_value,
                    new_component.data.transistor.type,
                    new_component.data.transistor.input_type
                );
                
                if (iid >= 0 && iid < component_array.size)
                {
                    Component *c = &component_array.data[iid];
                    switch(c->type)
                    {
                    case POWERSUPPLY:
                        c->data.powersupply.pin1 = component_array.size;
                        break;
                    case TRANSISTOR:
                        if (c->data.transistor.pin1 == -1) c->data.transistor.pin1 = component_array.size;
                        else if (c->data.transistor.pin2 == -1) c->data.transistor.pin2 = component_array.size;
                        else if (c->data.transistor.pin3 == -1) c->data.transistor.pin3 = component_array.size;
                        break;
                    case RESISTOR:
                        if (c->data.resistor.pin1 == -1) c->data.resistor.pin1 = component_array.size;
                        else if (c->data.resistor.pin2 == -1) c->data.resistor.pin2 = component_array.size;
                        break;
                    }
                }
                
                if (bid >= 0 && bid < component_array.size)
                {
                    Component *c = &component_array.data[bid];
                    switch(c->type)
                    {
                    case POWERSUPPLY:
                        c->data.powersupply.pin1 = component_array.size;
                        break;
                    case TRANSISTOR:
                        if (c->data.transistor.pin1 == -1) c->data.transistor.pin1 = component_array.size;
                        else if (c->data.transistor.pin2 == -1) c->data.transistor.pin2 = component_array.size;
                        else if (c->data.transistor.pin3 == -1) c->data.transistor.pin3 = component_array.size;
                        break;
                    case RESISTOR:
                        if (c->data.resistor.pin1 == -1) c->data.resistor.pin1 = component_array.size;
                        else if (c->data.resistor.pin2 == -1) c->data.resistor.pin2 = component_array.size;
                        break;
                    }
                }
                
                addComponent(&component_array, new_component);
                printf("Transistor added.\n");
                break;

            case 'D':
                int id = 0;
                double led_current = 0.0;
                
                list_components(&component_array);
                printf("Enter component ID to connect to LED: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin) && sscanf(input_buffer, "%d", &id) == 1)
                {
                    if (id >= 0 && id < component_array.size)
                    {
                        Component *c = &component_array.data[id];
                        switch (c->type) {
                        case POWERSUPPLY:
                            led_current = c->data.powersupply.voltage;
                            break;
                        case TRANSISTOR:
                            led_current = c->data.transistor.output;
                            break;
                        case RESISTOR:
                            led_current = c->data.resistor.output;
                            break;
                        }
                    }
                }
                
                printf("LED current: %.3f A - ", led_current);
                printf(led_bulb(led_current) ? "LED ON\n" : "LED OFF or Burned\n");
                break;

            case 'L':
                list_components(&component_array);
                break;

            case 'S':
                printf("Filename to save: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin))
                {
                    input_buffer[strcspn(input_buffer, "\n")] = 0;
                    saveCircuit(input_buffer, &component_array);
                }
                break;

            case 'F':
                printf("Filename to load: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin))
                {
                    input_buffer[strcspn(input_buffer, "\n")] = 0;
                    loadCircuit(input_buffer, &component_array);
                }
                break;

            case 'Q':
                quit = true;
                break;

            default:
                printf("Invalid option.\n");
                break;
        }
    }
    freeComponentArray(&component_array);
    printf("Goodbye!\n");
    return 0;
}