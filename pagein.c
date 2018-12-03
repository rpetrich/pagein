#include <mach/vm_map.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld_images.h>
#include <stdio.h>
#include <stdlib.h>

void usage(const char* self) {
	fprintf(stderr, "Usage: %s <PID>\n\tForce reads all pages mapped by a process into RAM\n", self);
}

int main(int argc, char **argv) {
	if (!argv[1]) {
		usage(argv[0]);
		return 1;
	}
	bool quiet = strcmp(argv[1], "-q") == 0;
	if (quiet && !argv[2]) {
		usage(argv[0]);
		return 1;
	}
	int pid = atoi(quiet ? argv[2] : argv[1]);
	mach_port_t	task;
	kern_return_t result = task_for_pid(mach_task_self(), pid, &task);
	if (result != KERN_SUCCESS) {
		fprintf(stderr, "Unable to load task for pid %d with error %lld\n", pid, (long long)result);
		return 1;
	}
	vm_region_basic_info_data_t info;
	mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
	mach_port_t object_name;
	mach_vm_address_t address = 0;
	mach_vm_size_t size = 0;
	uint64_t total = 0;
	if (!quiet) {
		do {
			result = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&info, &count, &object_name);
			if (result != KERN_SUCCESS) {
				break;
			}
			if (object_name != MACH_PORT_NULL) {
	  			mach_port_deallocate(mach_task_self(), object_name);
	  		}
	  		if (info.protection & VM_PROT_READ && !info.reserved) {
				total += size;
			}
			address += size;
			count = VM_REGION_BASIC_INFO_COUNT_64;
		} while(address != 0);
		address = 0;
		size = 0;
	}
	char temp = 0;
	int percent = 0;
	uint64_t progress = 0;
	do {
		result = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&info, &count, &object_name);
		if (result != KERN_SUCCESS) {
			if (result == 1) {
				break;
			}
			fprintf(stderr, "%sError reading region: %lld\n", quiet ? "" : "\r", (long long)result);
			return 1;
		}
		// fprintf(stderr, "address=%p size=%lld behavior=%i reserved=%i user_wired_count=%i\n", (void *)(uintptr_t)address, (long long)size, (int)info.behavior, (int)info.reserved, (int)info.user_wired_count);
  		if (info.protection & VM_PROT_READ && !info.reserved) {
			vm_prot_t cur_protection;
			vm_prot_t max_protection;
			mach_vm_address_t local_address = 0;
			result = mach_vm_remap(mach_task_self(), &local_address, size, 0, true, task, address, false, &cur_protection, &max_protection, VM_INHERIT_NONE);
			if (result != KERN_SUCCESS) {
				fprintf(stderr, "%sError mapping region: %lld\n", quiet ? "" : "\r", (long long)result);
				return 1;
			}
  			const char *buffer = (const char *)local_address;
  			for (int i = 0; i < size; i += 4096) {
  				temp ^= buffer[i];
  				if (!quiet) {
  					progress += 4096;
  					int new_percent = (progress * 100) / total;
  					if (new_percent != percent) {
  						percent = new_percent;
						fprintf(stderr, "\r%i%% %lld/%lld MB", percent, progress / (1024 * 1024), total / (1024 * 1024));
  					}
  				}
  			}
  			result = mach_vm_deallocate(mach_task_self(), local_address, size);
			if (result != KERN_SUCCESS) {
				fprintf(stderr, "%sUnable to unmap region: %lld\n", quiet ? "" : "\r", (long long)result);
				return 1;
			}
  		}
		if (object_name != MACH_PORT_NULL) {
  			mach_port_deallocate(mach_task_self(), object_name);
  		}
		address += size;
		count = VM_REGION_BASIC_INFO_COUNT_64;
	} while(address != 0);
	if (!quiet) {
		fprintf(stderr, "\r100%% %lld/%lld MB\n", total / (1024 * 1024), total / (1024 * 1024));
	}
	volatile char unused;
	unused = temp;
	return 0;
}
