/* If faulted due to page not present */

if (not_present) {
	/* Iterate through the current thread's supplemental page table to
	 find if the faulting address is valid. */
	found_valid = spt_present(t, pg_round_down(fault_addr));

	/* If it's in our suplemental page table */
	if (found_valid) {
		vma = spt_get_struct(t, pg_round_down(fault_addr));
		vma->pinned = true;
		new_page = palloc_get_page(PAL_USER);
		if (new_page == NULL) {
			new_page = frame_evict();
		}

		if (vma->pg_type == FILE_SYS) {
			/* Read the file into the kernel page. If we do not read the
			 PGSIZE bytes, then zero out the rest of the page. */
			/* Seek to the correct offset. */
			/* Checking for lock holder should be atomic */
			intr_disable();

			/* Only lock if we don't already have this lock */
			if (!lock_held_by_current_thread(&filesys_lock)) {
				lock_acquire(&filesys_lock);
				fs_lock = true;
			}
			intr_enable();

			file_seek(vma->vm_file, vma->ofs);
			/* Read from the file. */
			bytes_read = file_read(vma->vm_file, new_page, (off_t) vma->pg_read_bytes);

			if (fs_lock) {
				lock_release(&filesys_lock);
			}

			ASSERT(bytes_read == vma->pg_read_bytes);
			memset(new_page + bytes_read, 0, PGSIZE - bytes_read);
		} else if (vma->pg_type == ZERO) {
			/* Zero out the page. */
			memset(new_page, 0, PGSIZE);
		} else if (vma->pg_type == SWAP) {
			/* Read in from swap into the new page. */
			swap_remove(vma->swap_ind, new_page);
			vma->swap_ind = NULL;
			vma->pg_type = PMEM;
		}

		/* Record the new kpage in the vm_area_struct. */
		vma->kpage = new_page;

		/* Add the new page-frame mapping to the frame table. */
		frame_add(t, pg_round_down(fault_addr), new_page);
		if (!pagedir_set_page(t->pagedir, pg_round_down(fault_addr), new_page, vma->writable)) {
			kill(f);
		}
	} else {
		/* Handling stack extension */
		if ((fault_addr == esp - 4) || (fault_addr == esp - 32)) {
			/* Check for stack overflow */
			if (fault_addr < STACK_MIN) {
				exit(-1);
			}

			/* If we're here, let's give this process another page */
			new_page = palloc_get_page(PAL_ZERO | PAL_USER);
			if (new_page == NULL) {
				new_page = frame_evict();
			}
			if (!pagedir_set_page(t->pagedir, pg_round_down(fault_addr), new_page, 1)) {
				kill(f);
			}
			/* Record the new stack page in the supplemental page table and
			 the frame table. */
			vma = (struct vm_area_struct *) malloc(sizeof(struct vm_area_struct));
			vma->vm_start = pg_round_down(fault_addr);
			vma->vm_end = pg_round_down(fault_addr) + PGSIZE - sizeof(uint8_t);
			vma->kpage = new_page;
			vma->pg_read_bytes = NULL;
			vma->writable = true;
			vma->pinned = true;
			vma->vm_file = NULL;
			vma->ofs = NULL;
			vma->swap_ind = NULL;
			vma->pg_type = PMEM;
			spt_add(thread_current(), vma);
			frame_add(t, pg_round_down(fault_addr), new_page);
		}
		/* Other case of stack extension */
		else if (fault_addr >= esp) {
			new_page = palloc_get_page(PAL_ZERO | PAL_USER);

			if (new_page == NULL) {
				new_page = frame_evict();
			}

			if (!pagedir_set_page(t->pagedir, pg_round_down(fault_addr), new_page, 1)) {
				kill(f);
			}

			/* Record the new stack page in the supplemental page table and
			 the frame table. */
			vma = (struct vm_area_struct *) malloc(sizeof(struct vm_area_struct));
			vma->vm_start = pg_round_down(fault_addr);
			vma->vm_end = pg_round_down(fault_addr) + PGSIZE - sizeof(uint8_t);
			vma->kpage = new_page;
			vma->pg_read_bytes = NULL;
			vma->writable = true;
			vma->pinned = true;
			vma->vm_file = NULL;
			vma->ofs = NULL;
			vma->swap_ind = NULL;
			vma->pg_type = PMEM;
			spt_add(thread_current(), vma);
			frame_add(t, pg_round_down(fault_addr), new_page);
		}

		/* Else is probably an invalid access */
		else {
			exit(-1);
		}
	}
}

/* Rights violation */
else {
	exit(-1);
}
