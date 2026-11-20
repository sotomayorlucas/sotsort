Logsort (aphitorite, MIT, https://github.com/aphitorite/Logsort, commit 76a5e32) for operation
counts. Changes: `memcpy` of elements replaced by `LOG_COPY` (element-wise assignments, so
moves are counted) and a cast on `malloc` so it compiles as C++. The algorithm is unchanged.
