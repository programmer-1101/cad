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
    } output;
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

typedef struct Component
{
    ComponentData data;
    ComponentType type;
    struct Component **anode_connections;
    struct Component **cathode_connections;
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
        size_t new_capacity = arr->capacity * 2;
        if (new_capacity == 0)
        {
            new_capacity = 1;
        }

        Component *new_data = (Component *)realloc(arr->data, new_capacity * sizeof(Component));
        if (new_data == NULL)
        {
            perror("Failed to allocate memory for Dynamic array");
            exit(EXIT_FAILURE);
        }
        arr->data = new_data;
        arr->capacity = new_capacity;
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
        if (base_current > 0.00001)
        {
            if (return_collector_current)
                return transistor_beta * base_current;
            else
                return input_control_current_or_voltage + 0.7;
        }
    }
    else
    {
        if (base_current < -0.00001)
        {
            if (return_collector_current)
                return transistor_beta * -base_current;
            else
                return input_control_current_or_voltage - 0.7;
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
        printf("LED burn, current limit of 20mA exceeded: %fA\n", current);
        return 0;
    }
    else
        return 1;
}

void saveCircuit(char *file_name, ComponentArray *array)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *components_array = cJSON_CreateArray();

    if (!root || !components_array)
    {
        fprintf(stderr, "Error: Failed to create cJSON object/array.\n");
        cJSON_Delete(root);
        cJSON_Delete(components_array);
        return;
    }

    for (size_t i = 0; i < array->size; i++)
    {
        cJSON *component_obj = cJSON_CreateObject();
        if (!component_obj)
        {
            fprintf(stderr, "Error: Failed to create cJSON object for component.\n");
            cJSON_Delete(root);
            return;
        }

        cJSON_AddNumberToObject(component_obj, "id", array->data[i].data.powersupply.id);

        switch (array->data[i].type)
        {
        case POWERSUPPLY:
            cJSON_SetValuestring(cJSON_GetObjectItem(component_obj, "type"), "PowerSupply");
            cJSON_AddNumberToObject(component_obj, "voltage", array->data[i].data.powersupply.voltage);
            break;
        case RESISTOR:
            cJSON_SetValuestring(cJSON_GetObjectItem(component_obj, "type"), "Resistor");
            cJSON_AddNumberToObject(component_obj, "resistance", array->data[i].data.resistor.resistance);
            cJSON_AddStringToObject(component_obj, "output_type", array->data[i].data.resistor.output == CALC_VOLTAGE ? "Voltage" : "Current");
            break;
        case TRANSISTOR:
            cJSON_SetValuestring(cJSON_GetObjectItem(component_obj, "type"), "Transistor");
            cJSON_AddStringToObject(component_obj, "transistor_type", array->data[i].data.transistor.type == 0 ? "NPN" : "PNP");
            cJSON_AddStringToObject(component_obj, "input_output_format", array->data[i].data.transistor.input_type == 0 ? "Voltage" : "Current");
            break;
        default:
            fprintf(stderr, "Warning: Unknown component type encountered during saving.\n");
            cJSON_Delete(component_obj);
            continue;
        }
        cJSON_AddItemToArray(components_array, component_obj);
    }
    cJSON_AddItemToObject(root, "components", components_array);

    char *json_str = cJSON_Print(root);
    if (json_str == NULL)
    {
        fprintf(stderr, "Error: Failed to print cJSON to string.\n");
        cJSON_Delete(root);
        return;
    }

    FILE *fp = fopen(file_name, "w");
    if (fp == NULL)
    {
        perror("Error unable to open the file");
        cJSON_free(json_str);
        cJSON_Delete(root);
        return;
    }

    fputs(json_str, fp);
    fclose(fp);

    cJSON_free(json_str);
    cJSON_Delete(root);
    printf("Circuit saved to %s\n", file_name);
}

int main()
{
    ComponentArray component_array;
    component_array.size = 0;
    component_array.capacity = 1;
    component_array.data = (Component *)malloc(component_array.capacity * sizeof(Component));
    if (component_array.data == NULL)
    {
        perror("Failed to allocate memory for Dynamic array");
        exit(EXIT_FAILURE);
    }

    printf("Welcome to Electronic CAD\n");
    bool quit = false;
    char input_buffer[256];

    while (!quit)
    {
        char item_type_char;
        printf("\nEnter what to do\n");
        printf("A: Power Supply\nB: Resistor\nC: Transistor\nD: LED\nL: List Components\nS: Save\nF: Load From File:\nQ: Quit\n");

        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
        {
            fprintf(stderr, "Error reading input.\n");
            break;
        }
        if (sscanf(input_buffer, " %c", &item_type_char) != 1)
        {
            fprintf(stderr, "Invalid input. Please try again.\n");
            continue;
        }

        item_type_char = toupper(item_type_char);

        Component new_component = {0};

        switch (item_type_char)
        {
        case 'A':
            printf("Enter Voltage (V):\t");
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL || sscanf(input_buffer, "%lf", &new_component.data.powersupply.voltage) != 1)
            {
                fprintf(stderr, "Invalid voltage input. Please try again.\n");
                continue;
            }
            new_component.data.powersupply.id = component_array.size;
            new_component.type = POWERSUPPLY;
            addComponent(&component_array, new_component);
            printf("Power Supply added with ID: %d, Voltage: %.2fV\n", new_component.data.powersupply.id, new_component.data.powersupply.voltage);
            break;
        case 'B':
            printf("Enter resistance (Ohms):\t");
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL || sscanf(input_buffer, "%lf", &new_component.data.resistor.resistance) != 1)
            {
                fprintf(stderr, "Invalid resistance input. Please try again.\n");
                continue;
            }
            new_component.data.resistor.id = component_array.size;
            new_component.type = RESISTOR;
            printf("Enter output type (0 for Calculate Voltage, 1 for Calculate Current):\t");
            int resistor_output_type_int;
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL || sscanf(input_buffer, "%d", &resistor_output_type_int) != 1 || (resistor_output_type_int != 0 && resistor_output_type_int != 1))
            {
                fprintf(stderr, "Invalid output type. Using default (Calculate Voltage).\n");
                new_component.data.resistor.output = CALC_VOLTAGE;
            }
            else
            {
                new_component.data.resistor.output = (resistor_output_type_int == 0) ? CALC_VOLTAGE : CALC_CURRENT;
            }
            addComponent(&component_array, new_component);
            printf("Resistor added with ID: %d, Resistance: %.2f Ohms\n", new_component.data.resistor.id, new_component.data.resistor.resistance);
            break;

        case 'C':
        {
            int type_int;
            int input_type_int;

            printf("Enter Transistor type (NPN (0) or PNP (1)):\t");
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL || sscanf(input_buffer, "%d", &type_int) != 1 || (type_int != 0 && type_int != 1))
            {
                fprintf(stderr, "Invalid transistor type input. Please enter 0 or 1.\n");
                continue;
            }
            new_component.data.transistor.type = (bool)type_int;

            printf("Enter the expected input format for simulation (Voltage (0) or Current (1)):\t");
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL || sscanf(input_buffer, "%d", &input_type_int) != 1 || (input_type_int != 0 && input_type_int != 1))
            {
                fprintf(stderr, "Invalid input format input. Please enter 0 or 1.\n");
                continue;
            }
            new_component.data.transistor.input_type = (bool)input_type_int;
            new_component.data.transistor.id = component_array.size;
            new_component.type = TRANSISTOR;
            addComponent(&component_array, new_component);
            printf("Transistor added with ID: %d, Type: %s, Input Format: %s\n",
                   new_component.data.transistor.id,
                   new_component.data.transistor.type ? "PNP" : "NPN",
                   new_component.data.transistor.input_type ? "Current" : "Voltage");
            break;
        }
        case 'D':
            printf("LED is a simulation function. To simulate an LED, you'll need to provide current.\n");
            printf("Enter current for LED simulation (A):\t");
            double led_current;
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL || sscanf(input_buffer, "%lf", &led_current) != 1)
            {
                fprintf(stderr, "Invalid current input for LED. Please try again.\n");
                continue;
            }
            if (led_bulb(led_current))
            {
                printf("LED is ON!\n");
            }
            else
            {
                printf("LED is OFF or Burned.\n");
            }
            break;
        case 'S':
        {
            if (component_array.size == 0)
            {
                printf("No components to save.\n");
                break;
            }
            char save_file_name[256];
            printf("Enter filename to save (e.g., circuit.json):\t");
            if (fgets(save_file_name, sizeof(save_file_name), stdin) == NULL)
            {
                fprintf(stderr, "Error reading filename.\n");
                break;
            }
            save_file_name[strcspn(save_file_name, "\n")] = 0;
            saveCircuit(save_file_name, &component_array);
            break;
        }
        case 'Q':
            quit = true;
            break;

        case 'L':
            if (component_array.size == 0)
            {
                printf("No components in the circuit yet.\n");
            }
            else
            {
                printf("\n--- Current Circuit Components ---\n");
                for (size_t i = 0; i < component_array.size; i++)
                {
                    Component current_comp = getComponent(&component_array, i);
                    printf("ID: %d, Type: ", current_comp.data.powersupply.id);
                    switch (current_comp.type)
                    {
                    case POWERSUPPLY:
                        printf("Power Supply, Voltage: %.2fV\n", current_comp.data.powersupply.voltage);
                        break;
                    case RESISTOR:
                        printf("Resistor, Resistance: %.2f Ohms, Output: %s\n",
                               current_comp.data.resistor.resistance,
                               current_comp.data.resistor.output == CALC_VOLTAGE ? "Voltage" : "Current");
                        break;
                    case TRANSISTOR:
                        printf("Transistor, Type: %s, Input Format: %s\n",
                               current_comp.data.transistor.type ? "PNP" : "NPN",
                               current_comp.data.transistor.input_type ? "Current" : "Voltage");
                        break;
                    }
                }
                printf("----------------------------------\n");
            }
            break;

        case 'F':
            char load_file_name[256];
            printf("Enter filename to save (e.g., circuit.json):\t");
            if (fgets(load_file_name, sizeof(load_file_name), stdin) == NULL)
            {
                fprintf(stderr, "Error reading filename.\n");
                break;
            }
            load_file_name[strcspn(load_file_name, "\n")] = 0;

            FILE *fp = fopen(load_file_name, "r");
            if (fp == NULL)
            {
                printf("Error: Unable to open the file.\n");
                return 1;
            }

            // read the file contents into a string
            fseek(fp, 0, SEEK_END);
            long filesize = ftell(fp);
            rewind(fp);

            char *buffer = malloc(filesize + 1);
            fread(buffer, 1, filesize, fp);
            buffer[filesize] = '\0';
            fclose(fp);

            // parse the JSON data
            cJSON *json = cJSON_Parse(buffer);

            free(buffer);

            if (json == NULL)
            {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL)
                {
                    printf("Error: %s\n", error_ptr);
                }
                return 1;
            }

            printf("%s", json->type);

            // Clear existing data
            freeComponentArray(&component_array);
            component_array.size = 0;
            component_array.capacity = 1;
            component_array.data = (Component *)malloc(component_array.capacity * sizeof(Component));

            cJSON *components = cJSON_GetObjectItem(json, "components");
            if (!cJSON_IsArray(components))
            {
                printf("Invalid format: 'components' is not an array.\n");
                cJSON_Delete(json);
                break;
            }

            cJSON *component_json = NULL;
            cJSON_ArrayForEach(component_json, components)
            {
                Component new_component = {0};

                cJSON *type = cJSON_GetObjectItem(component_json, "type");
                cJSON *id = cJSON_GetObjectItem(component_json, "id");

                if (!cJSON_IsString(type) || !cJSON_IsNumber(id))
                    continue;

                if (strcmp(type->valuestring, "PowerSupply") == 0)
                {
                    new_component.type = POWERSUPPLY;
                    new_component.data.powersupply.id = id->valueint;
                    new_component.data.powersupply.voltage = cJSON_GetObjectItem(component_json, "voltage")->valuedouble;
                }
                else if (strcmp(type->valuestring, "Resistor") == 0)
                {
                    new_component.type = RESISTOR;
                    new_component.data.resistor.id = id->valueint;
                    new_component.data.resistor.resistance = cJSON_GetObjectItem(component_json, "resistance")->valuedouble;

                    cJSON *output_type = cJSON_GetObjectItem(component_json, "output_type");
                    if (cJSON_IsString(output_type) && strcmp(output_type->valuestring, "Current") == 0)
                        new_component.data.resistor.output = CALC_CURRENT;
                    else
                        new_component.data.resistor.output = CALC_VOLTAGE;
                }
                else if (strcmp(type->valuestring, "Transistor") == 0)
                {
                    new_component.type = TRANSISTOR;
                    new_component.data.transistor.id = id->valueint;

                    cJSON *trans_type = cJSON_GetObjectItem(component_json, "transistor_type");
                    cJSON *io_format = cJSON_GetObjectItem(component_json, "input_output_format");

                    new_component.data.transistor.type = (cJSON_IsString(trans_type) && strcmp(trans_type->valuestring, "PNP") == 0);
                    new_component.data.transistor.input_type = (cJSON_IsString(io_format) && strcmp(io_format->valuestring, "Current") == 0);
                }

                addComponent(&component_array, new_component);
            }

            cJSON_Delete(json);
            printf("Loaded %zu components from file.\n", component_array.size);

            printf("\n--- Current Circuit Components ---\n");
            for (size_t i = 0; i < component_array.size; i++)
            {
                Component current_comp = getComponent(&component_array, i);
                printf("ID: ");
                switch (current_comp.type)
                {
                case POWERSUPPLY:
                    printf("%d, Type: Power Supply, Voltage: %.2fV\n",
                           current_comp.data.powersupply.id,
                           current_comp.data.powersupply.voltage);
                    break;
                case RESISTOR:
                    printf("%d, Type: Resistor, Resistance: %.2f Ohms, Output: %s\n",
                           current_comp.data.resistor.id,
                           current_comp.data.resistor.resistance,
                           current_comp.data.resistor.output == CALC_VOLTAGE ? "Voltage" : "Current");
                    break;
                case TRANSISTOR:
                    printf("%d, Type: Transistor, Type: %s, Input Format: %s\n",
                           current_comp.data.transistor.id,
                           current_comp.data.transistor.type ? "PNP" : "NPN",
                           current_comp.data.transistor.input_type ? "Current" : "Voltage");
                    break;
                }

                switch (current_comp.type)
                {
                case POWERSUPPLY:
                    printf("Power Supply, Voltage: %.2fV\n", current_comp.data.powersupply.voltage);
                    break;
                case RESISTOR:
                    printf("Resistor, Resistance: %.2f Ohms, Output: %s\n",
                           current_comp.data.resistor.resistance,
                           current_comp.data.resistor.output == CALC_VOLTAGE ? "Voltage" : "Current");
                    break;
                case TRANSISTOR:
                    printf("Transistor, Type: %s, Input Format: %s\n",
                           current_comp.data.transistor.type ? "PNP" : "NPN",
                           current_comp.data.transistor.input_type ? "Current" : "Voltage");
                    break;
                    
                }
            }
            printf("----------------------------------\n");
            break;
        default:
            printf("A: Power Supply\nB: Resistor\nC: Transistor\nD: LED\nL: List Components\nS: Save\nQ: Quit\n");
            break;
        }
    }

    freeComponentArray(&component_array);
    printf("Exiting Electronic CAD. Goodbye!\n");
    return 0;
}