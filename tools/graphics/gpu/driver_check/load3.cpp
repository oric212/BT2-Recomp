// load3 <shader.spv> <isa output prefix>: runs one 24-bit read shader over 512 x 224 packed pixels on the first
// discrete GPU, counts the wrong words (and those that equal the read from the address rounded down to 4), and
// writes the driver's compiled code for it (VK_AMD_shader_info disassembly, VK_KHR_pipeline_executable_properties).
// check_unaligned_load.py next to it builds and runs it (docs/plans/graphics/GPU_COMPATIBILITY.md).
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define CHECK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { std::fprintf(stderr, "%s failed: %d\n", #x, int(r_)); std::exit(2); } } while (0)

static std::vector<char> readFile(const char *path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { std::fprintf(stderr, "cannot read %s\n", path); std::exit(2); }
	return std::vector<char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct Buffer { VkBuffer buffer; VkDeviceMemory memory; void *mapped; VkDeviceAddress address; };

int main(int argc, char **argv)
{
	if (argc < 3) { std::fprintf(stderr, "usage: load3 <shader.spv> <isa prefix>\n"); return 2; }
	const auto spirv = readFile(argv[1]);

	VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app.apiVersion = VK_API_VERSION_1_3;
	VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	ici.pApplicationInfo = &app;
	VkInstance instance;
	CHECK(vkCreateInstance(&ici, nullptr, &instance));

	uint32_t count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	std::vector<VkPhysicalDevice> gpus(count);
	vkEnumeratePhysicalDevices(instance, &count, gpus.data());
	VkPhysicalDevice gpu = VK_NULL_HANDLE;
	VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
	VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driver};
	for (auto g : gpus)
	{
		vkGetPhysicalDeviceProperties2(g, &props);
		if (props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { gpu = g; break; }
	}
	if (!gpu) { std::fprintf(stderr, "no discrete GPU\n"); return 2; }
	std::printf("%s, %s %s\n", props.properties.deviceName, driver.driverName, driver.driverInfo);

	VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR exec_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR};
	VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &exec_features};
	VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f12};
	vkGetPhysicalDeviceFeatures2(gpu, &f2);
	if (!f12.bufferDeviceAddress || !f12.scalarBlockLayout || !f12.storageBuffer8BitAccess) { std::fprintf(stderr, "features missing\n"); return 2; }
	VkPhysicalDeviceVulkan12Features want12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	want12.bufferDeviceAddress = VK_TRUE;
	want12.scalarBlockLayout = VK_TRUE;
	want12.storageBuffer8BitAccess = VK_TRUE;
	want12.shaderInt8 = f12.shaderInt8;
	VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR want_exec{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR};
	want_exec.pipelineExecutableInfo = exec_features.pipelineExecutableInfo;
	want12.pNext = &want_exec;

	uint32_t family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &family_count, nullptr);
	std::vector<VkQueueFamilyProperties> families(family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &family_count, families.data());
	uint32_t family = 0;
	while (!(families[family].queueFlags & VK_QUEUE_COMPUTE_BIT)) family++;
	float priority = 1.0f;
	VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	qci.queueFamilyIndex = family;
	qci.queueCount = 1;
	qci.pQueuePriorities = &priority;
	std::vector<const char *> extensions = {VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, VK_AMD_SHADER_INFO_EXTENSION_NAME};
	if (exec_features.pipelineExecutableInfo) extensions.push_back(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
	VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &want12};
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = uint32_t(extensions.size());
	dci.ppEnabledExtensionNames = extensions.data();
	VkDevice device;
	CHECK(vkCreateDevice(gpu, &dci, nullptr, &device));
	VkQueue queue;
	vkGetDeviceQueue(device, family, 0, &queue);

	VkPhysicalDeviceMemoryProperties memory_props;
	vkGetPhysicalDeviceMemoryProperties(gpu, &memory_props);
	auto makeBuffer = [&](VkDeviceSize size) {
		Buffer b{};
		VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bci.size = size;
		bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		CHECK(vkCreateBuffer(device, &bci, nullptr, &b.buffer));
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(device, b.buffer, &req);
		uint32_t type = 0;
		const VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		while (!((req.memoryTypeBits >> type) & 1) || (memory_props.memoryTypes[type].propertyFlags & flags) != flags) type++;
		VkMemoryAllocateFlagsInfo afi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
		afi.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
		VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &afi};
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = type;
		CHECK(vkAllocateMemory(device, &mai, nullptr, &b.memory));
		CHECK(vkBindBufferMemory(device, b.buffer, b.memory, 0));
		CHECK(vkMapMemory(device, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped));
		VkBufferDeviceAddressInfo dai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
		dai.buffer = b.buffer;
		b.address = vkGetBufferDeviceAddress(device, &dai);
		return b;
	};

	const uint32_t pixels = 512 * 224;
	Buffer source = makeBuffer(pixels * 3 + 64), dest = makeBuffer(pixels * 4);
	auto *bytes = static_cast<uint8_t *>(source.mapped);
	for (uint32_t k = 0; k < pixels * 3 + 64; k++) { uint32_t h = k * 0x9E3779B1u; h ^= h >> 15; bytes[k] = uint8_t(h * 0x2C1B3C6Du >> 24); }
	std::memset(dest.mapped, 0xEE, pixels * 4);

	VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
	VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	dslci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	dslci.bindingCount = 1;
	dslci.pBindings = &binding;
	VkDescriptorSetLayout set_layout;
	CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &set_layout));
	VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, 20};
	VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &set_layout;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &range;
	VkPipelineLayout layout;
	CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &layout));
	VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	smci.codeSize = spirv.size();
	smci.pCode = reinterpret_cast<const uint32_t *>(spirv.data());
	VkShaderModule module;
	CHECK(vkCreateShaderModule(device, &smci, nullptr, &module));
	VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	if (exec_features.pipelineExecutableInfo)
		cpci.flags = VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR | VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;
	cpci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main"};
	cpci.layout = layout;
	VkPipeline pipeline;
	CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline));

	// The driver's code for the shader.
	const std::string prefix = argv[2];
	auto getShaderInfo = reinterpret_cast<PFN_vkGetShaderInfoAMD>(vkGetDeviceProcAddr(device, "vkGetShaderInfoAMD"));
	size_t size = 0;
	if (getShaderInfo && getShaderInfo(device, pipeline, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD, &size, nullptr) == VK_SUCCESS && size)
	{
		std::vector<char> text(size);
		getShaderInfo(device, pipeline, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD, &size, text.data());
		std::ofstream(prefix + "-amd-disassembly.txt", std::ios::binary).write(text.data(), std::streamsize(std::strlen(text.data())));
		std::printf("VK_AMD_shader_info disassembly: %zu bytes\n", size);
	}
	else
		std::printf("VK_AMD_shader_info disassembly: not available\n");
	if (exec_features.pipelineExecutableInfo)
	{
		auto getExecutables = reinterpret_cast<PFN_vkGetPipelineExecutablePropertiesKHR>(vkGetDeviceProcAddr(device, "vkGetPipelineExecutablePropertiesKHR"));
		auto getRepresentations = reinterpret_cast<PFN_vkGetPipelineExecutableInternalRepresentationsKHR>(vkGetDeviceProcAddr(device, "vkGetPipelineExecutableInternalRepresentationsKHR"));
		VkPipelineInfoKHR pinfo{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
		pinfo.pipeline = pipeline;
		uint32_t executables = 0;
		getExecutables(device, &pinfo, &executables, nullptr);
		for (uint32_t e = 0; e < executables; e++)
		{
			VkPipelineExecutableInfoKHR einfo{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR};
			einfo.pipeline = pipeline;
			einfo.executableIndex = e;
			uint32_t reps = 0;
			getRepresentations(device, &einfo, &reps, nullptr);
			std::vector<VkPipelineExecutableInternalRepresentationKHR> list(reps, {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR});
			getRepresentations(device, &einfo, &reps, list.data());
			std::vector<std::vector<char>> storage(reps);
			for (uint32_t r = 0; r < reps; r++) { storage[r].resize(list[r].dataSize + 1); list[r].pData = storage[r].data(); }
			getRepresentations(device, &einfo, &reps, list.data());
			for (uint32_t r = 0; r < reps; r++)
			{
				std::string name = list[r].name;
				for (auto &c : name) if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
				std::ofstream out(prefix + "-exec" + std::to_string(e) + "-" + name + (list[r].isText ? ".txt" : ".bin"), std::ios::binary);
				out.write(static_cast<const char *>(list[r].pData), std::streamsize(list[r].isText ? std::strlen(static_cast<const char *>(list[r].pData)) : list[r].dataSize));
				std::printf("internal representation %u.%u: %s (%s), %zu bytes\n", e, r, list[r].name, list[r].description, list[r].dataSize);
			}
		}
	}

	// Run it.
	VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	cpi.queueFamilyIndex = family;
	VkCommandPool pool;
	CHECK(vkCreateCommandPool(device, &cpi, nullptr, &pool));
	VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	cbai.commandPool = pool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	VkCommandBuffer cmd;
	CHECK(vkAllocateCommandBuffers(device, &cbai, &cmd));
	VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	CHECK(vkBeginCommandBuffer(cmd, &begin));
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	struct { VkDeviceAddress source, dest; uint32_t count; } push{source.address, dest.address, pixels};
	vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 20, &push);
	VkDescriptorBufferInfo dbi{source.buffer, 0, VK_WHOLE_SIZE};
	VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &dbi;
	auto pushDescriptor = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR"));
	pushDescriptor(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &write);
	vkCmdDispatch(cmd, (pixels + 63) / 64, 1, 1);
	VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	CHECK(vkEndCommandBuffer(cmd));
	VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;
	CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
	CHECK(vkQueueWaitIdle(queue));

	const auto *words = static_cast<const uint32_t *>(dest.mapped);
	uint32_t wrong = 0, rounded = 0, by_offset[4] = {}, wrong_by_offset[4] = {};
	auto at = [&](uint32_t k) { return uint32_t(bytes[k]) | (uint32_t(bytes[k + 1]) << 8) | (uint32_t(bytes[k + 2]) << 16); };
	for (uint32_t i = 0; i < pixels; i++)
	{
		const uint32_t k = i * 3, want = at(k);
		by_offset[k & 3]++;
		if (words[i] != want)
		{
			wrong++;
			wrong_by_offset[k & 3]++;
			if (words[i] == at(k & ~3u)) rounded++;
			if (wrong <= 4) std::printf("  pixel %u (byte %u): 0x%06x, not 0x%06x\n", i, k, words[i], want);
		}
	}
	std::printf("%s: %u of %u words wrong; %u of them equal the read from the address rounded down to 4\n", argv[1], wrong, pixels, rounded);
	for (int o = 0; o < 4; o++) std::printf("  address mod 4 = %d: %u of %u wrong\n", o, wrong_by_offset[o], by_offset[o]);
	vkDeviceWaitIdle(device);
	return wrong ? 1 : 0;
}
