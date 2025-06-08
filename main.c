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
 Remove-Item $env:LOCALAPPDATA\nvim\.git -Recurse -Force   double output;
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

double resistor(double resistance, double input, bool type)
{
    return type ? (input * resistance) : (input / resistance);
}

double transistor(double control, double base, bool is_NPN, bool return_collector)
{
    double beta = 100.0;
    if (is_NPN && base > 0.00001)
        return return_collector ? beta * base : control + 0.7;
    else if (!is_NPN && base < -0.00001)
        return return_collector ? beta * -base : control - 0.7;
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

        cJSON_AddNumberToObject(obj, "id", comp->data.powersupply.id);

        switch (comp->type)
        {
        case POWERSUPPLY:
            cJSON_AddStringToObject(obj, "type", "PowerSupply");
            cJSON_AddNumberToObject(obj, "voltage", comp->data.powersupply.voltage);
            break;
        case RESISTOR:
            cJSON_AddStringToObject(obj, "type", "Resistor");
            cJSON_AddNumberToObject(obj, "resistance", comp->data.resistor.resistance);
            cJSON_AddStringToObject(obj, "output_type", comp->data.resistor.output == CALC_VOLTAGE ? "Voltage" : "Current");
            break;
        case TRANSISTOR:
            cJSON_AddStringToObject(obj, "type", "Transistor");
            cJSON_AddStringToObject(obj, "transistor_type", comp->data.transistor.type ? "PNP" : "NPN");
            cJSON_AddStringToObject(obj, "input_output_format", comp->data.transistor.input_type ? "Current" : "Voltage");
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
    array->capacity = 1;
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
        }
        else if (strcmp(type->valuestring, "Resistor") == 0)
        {
            new_comp.type = RESISTOR;
            new_comp.data.resistor.id = id->valueint;
            new_comp.data.resistor.resistance = cJSON_GetObjectItem(item, "resistance")->valuedouble;
            cJSON *out = cJSON_GetObjectItem(item, "output_type");
            new_comp.data.resistor.output = (out && strcmp(out->valuestring, "Current") == 0) ? CALC_CURRENT : CALC_VOLTAGE;
        }
        else if (strcmp(type->valuestring, "Transistor") == 0)
        {
            new_comp.type = TRANSISTOR;
            new_comp.data.transistor.id = id->valueint;
            new_comp.data.transistor.type = strcmp(cJSON_GetObjectItem(item, "transistor_type")->valuestring, "PNP") == 0;
            new_comp.data.transistor.input_type = strcmp(cJSON_GetObjectItem(item, "input_output_format")->valuestring, "Current") == 0;
        }

        addComponent(array, new_comp);
    }

    cJSON_Delete(root);
    printf("Circuit loaded from '%s'\n", file_name);
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
                int iid = 0;

                printf("0: Calc Voltage, 1: Calc Current: ");
                if (fgets(input_buffer, sizeof(input_buffer), stdin))
                    sscanf(input_buffer, "%d", &otype);
                new_component.data.resistor.output = (otype == 1) ? CALC_CURRENT : CALC_VOLTAGE;

                for(int i = 0; i < component_array.size; i++)
                {
                    Component *c = &component_array.data[i];
                    printf("ID %d - ", i);

                    switch(c->type)
                    {
                        case POWERSUPPLY:
                            printf("PowerSupply voltage: %d", -c->data.powersupply.voltage);
                            printf("PowerSupply voltage: %d", c->data.powersupply.voltage);
                            break;
                        case TRANSISTOR:
                            printf("Transistor output: %d", c->data.transistor.output);
                            break;
                        case RESISTOR:
                            printf("Resistor output: %d", c->data.resistor.output);
                            break;
                    }
                }

                printf("Enter Component id:");
                if (fgets(input_buffer, sizeof(input_buffer), stdin))
                    sscanf(input_buffer, "%d", &iid);
                Component *c = &component_array.data[iid];
                new_component.data.resistor.pin1 = iid;
                switch(c->type)
                {
                    case POWERSUPPLY:
                        new_component.data.resistor.input = c->data.powersupply.voltage;
                        new_component.data.resistor.output = resistor(new_component.data.resistor.resistance, c->data.powersupply.voltage, otype);
                        break;
                    case TRANSISTOR:
                        new_component.data.resistor.input = c->data.transistor.output;
                        new_component.data.resistor.output = resistor(new_component.data.resistor.resistance, c->data.transistor.output, otype);
                        c->data.transistor.pin3 = component_array.size;
                        break;
                    case RESISTOR:

                        new_component.data.resistor.input = c->data.resistor.output;
                        new_component.data.resistor.output = resistor(new_component.data.resistor.resistance, c->data.resistor.output, otype);
                        c->data.resistor.pin2 = component_array.size;
                        break;
                }
                
                addComponent(&component_array, new_component);
                printf("Resistor added.\n");
            }
            break;

        case 'C':
            printf("0: NPN, 1: PNP: ");
            int ttype = 0, io = 0, iid = 0, bid = 0;
            if (fgets(input_buffer, sizeof(input_buffer), stdin))
                sscanf(input_buffer, "%d", &ttype);
            printf("0: Voltage input, 1: Current input: ");
            if (fgets(input_buffer, sizeof(input_buffer), stdin))
                sscanf(input_buffer, "%d", &io);

            printf("Enter input voltage: ");
            for(int i = 0; i < component_array.size; i++)
            {
                Component *c = &component_array.data[i];
                printf("ID %d - ", i);

                switch(c->type)
                {
                    case POWERSUPPLY:
                        printf("PowerSupply voltage: %d", -c->data.powersupply.voltage);
                        printf("PowerSupply voltage: %d", c->data.powersupply.voltage);
                        break;
                    case TRANSISTOR:
                        printf("Transistor output: %d", c->data.transistor.output);
                        break;
                    case RESISTOR:
                        printf("Resistor output: %d", c->data.resistor.output);
                        break;
                }
            }
            if (fgets(input_buffer, sizeof(input_buffer), stdin))
                sscanf(input_buffer, "%d", &iid);

            printf("Enter base voltage: ");
            for(int i = 0; i < component_array.size; i++)
            {
                Component *c = &component_array.data[i];
                printf("ID %d - ", i);

                switch(c->type)
                {
                    case POWERSUPPLY:
                        printf("PowerSupply voltage: %d", -c->data.powersupply.voltage);
                        printf("PowerSupply voltage: %d", c->data.powersupply.voltage);
                        break;
                    case TRANSISTOR:
                        printf("Transistor output: %d", c->data.transistor.output);
                        break;
                    case RESISTOR:
                        printf("Resistor output: %d", c->data.resistor.output);
                        break;
                }
            }
            if (fgets(input_buffer, sizeof(input_buffer), stdin))
                sscanf(input_buffer, "%d", &bid);

            new_component.data.transistor.id = component_array.size;
            new_component.data.transistor.type = ttype;
            new_component.data.transistor.input_type = io;
            new_component.data.transistor.pin1 = iid;
            new_component.data.transistor.pin2 = bid;

            Component *c = &component_array.data[iid];
            switch(c->type)
            {
                case POWERSUPPLY:
                    new_component.data.transistor.input = c->data.powersupply.voltage;
                    break;
                case TRANSISTOR:
                    new_component.data.transistor.input = c->data.transistor.output;
                    break;
                case RESISTOR:
                    new_component.data.transistor.input = c->data.resistor.output;
                    break;
            }

            c = &component_array.data[bid];
            switch(c->type)
            {
                case POWERSUPPLY:
                    new_component.data.transistor.base = c->data.powersupply.voltage;
                    break;
                case TRANSISTOR:
                    new_component.data.transistor.base = c->data.transistor.output;
                    break;
                case RESISTOR:
                    new_component.data.transistor.base = c->data.resistor.output;
                    break;
            }

            new_component.data.transistor.output = transistor(new_component.data.transistor.input, new_component.data.transistor.base, new_component.data.transistor.type, new_component.data.transistor.input_type);
            new_component.type = TRANSISTOR;
            addComponent(&component_array, new_component);
            printf("Transistor added.\n");
            break;

        case 'D':
            printf("Enter current for LED (A): ");
            double led_current;
            if (fgets(input_buffer, sizeof(input_buffer), stdin) && sscanf(input_buffer, "%lf", &led_current) == 1)
                printf(led_bulb(led_current) ? "LED ON\n" : "LED OFF or Burned\n");
            break;

        case 'L':
            for (size_t i = 0; i < component_array.size; i++)

            {
                Component *c = &component_array.data[i];
                printf("ID %d - ", c->data.powersupply.id);
                switch (c->type)
                {
                case POWERSUPPLY:
                    printf("PowerSupply: %.2fV\n", c->data.powersupply.voltage);
                    break;
                case RESISTOR:
                    printf("Resistor: %.2f Ohms (%s)\n", c->data.resistor.resistance,
                           c->data.resistor.output == CALC_VOLTAGE ? "Voltage" : "Current");
                    break;
                case TRANSISTOR:
                    printf("Transistor: %s, Input: %s\n",
                           c->data.transistor.type ? "PNP" : "NPN",
                           c->data.transistor.input_type ? "Current" : "Voltage");
                    break;
                }
            }
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
