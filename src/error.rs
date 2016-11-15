/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

use std::error::Error;

#[repr(i32)]
pub enum GitError {
    Net = 12,
}

extern {
    // Set the libgit2 thread-local error
    fn giterr_set(klass: i32, message: *const u8);
}

pub fn set_error<T: Error>(klass: GitError, e: T) {
    unsafe {
        giterr_set(klass as i32, e.description().as_ptr());
    }
}
