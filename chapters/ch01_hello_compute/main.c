/**
 * VKCompute - Chapter 01: Hello Vulkan Compute
 */

#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Chapter 01: Hello Vulkan Compute\n");
    printf("==============================================\n\n");
    
    // ========================================================================
    // Step 1: Create a Vulkan Instance
    // ========================================================================
    
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "CH01 Hello Compute",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2
    };
    
    // On macOS, we need the portability extension for MoltenVK
#ifdef __APPLE__
    const char* extensions[] = {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = extensions
    };
#else
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };
#endif
    
    VkInstance instance;
    VkResult result = vkCreateInstance(&instance_info, NULL, &instance);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance! Error: %d\n", result);
        return 1;
    }
    printf("[ok] Vulkan instance created\n");
    
    // ========================================================================
    // Step 2: Find a Physical Device (GPU)
    // ========================================================================
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    
    if (device_count == 0) {
        fprintf(stderr, "No GPUs with Vulkan support found!\n");
        vkDestroyInstance(instance, NULL);
        return 1;
    }
    printf("[ok] Found %u GPU(s)\n", device_count);
    
    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    
    // Print info about each GPU and pick the first one
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    uint32_t compute_queue_family = UINT32_MAX;
    
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        
        const char* device_type = "Unknown";
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   device_type = "Discrete GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            device_type = "CPU"; break;
            default: break;
        }
        
        printf("\n  GPU %u: %s\n", i, props.deviceName);
        printf("    Type: %s\n", device_type);
        printf("    API Version: %u.%u.%u\n",
               VK_VERSION_MAJOR(props.apiVersion),
               VK_VERSION_MINOR(props.apiVersion),
               VK_VERSION_PATCH(props.apiVersion));
        
        // Find a queue family that supports compute
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, NULL);
        
        VkQueueFamilyProperties* queue_families = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, queue_families);
        
        for (uint32_t j = 0; j < queue_family_count; j++) {
            if (queue_families[j].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                printf("    Queue family %u: supports COMPUTE (%u queues)\n", 
                       j, queue_families[j].queueCount);
                if (physical_device == VK_NULL_HANDLE) {
                    physical_device = devices[i];
                    compute_queue_family = j;
                }
            }
        }
        free(queue_families);
    }
    free(devices);
    
    if (physical_device == VK_NULL_HANDLE) {
        fprintf(stderr, "No GPU with compute support found!\n");
        vkDestroyInstance(instance, NULL);
        return 1;
    }
    printf("\n[ok] Selected GPU with compute queue family %u\n", compute_queue_family);
    
    // ========================================================================
    // Step 3: Create a Logical Device
    // ========================================================================
    
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = compute_queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };
    
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info
    };
    
    VkDevice device;
    result = vkCreateDevice(physical_device, &device_info, NULL, &device);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device! Error: %d\n", result);
        vkDestroyInstance(instance, NULL);
        return 1;
    }
    printf("[ok] Logical device created\n");
    
    // ========================================================================
    // Step 4: Get the Compute Queue
    // ========================================================================
    
    VkQueue compute_queue;
    vkGetDeviceQueue(device, compute_queue_family, 0, &compute_queue);
    printf("[ok] Compute queue retrieved\n");
    
    // ========================================================================
    // Done! We have a working Vulkan setup.
    // ========================================================================
    
    printf("\n=== SUCCESS ===\n");
    printf("Vulkan is initialized and ready for compute work!\n");
    
    // ========================================================================
    // Cleanup
    // ========================================================================
    
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    printf("\n[ok] Cleanup complete\n");
    
    printf("\n=== What we learned ===\n");
    printf("1. Create a Vulkan instance (our connection to Vulkan)\n");
    printf("2. Enumerate and select a physical device (GPU)\n");
    printf("3. Find a queue family that supports compute operations\n");
    printf("4. Create a logical device with a compute queue\n");
    printf("\nNext chapter: Buffers - sending data to the GPU!\n");
    
    return 0;
}
