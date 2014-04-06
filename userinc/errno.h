#ifndef ERRNO_H
#define ERRNO_H

// We don't do errno. This definition doesn't allow it to be set, but never mind

#define errno (0)

#define strerror(err) ""

#define ENOMEM (-4)
#define EINVAL (-6)

#endif
