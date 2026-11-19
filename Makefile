CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -march=native -Wall -Wextra
test:
	$(CXX) $(CXXFLAGS) tests/test.cpp -o /tmp/sot_test -lpthread && /tmp/sot_test 3000 400
	$(CXX) $(CXXFLAGS) tests/test_extra.cpp -o /tmp/sot_extra -lpthread && /tmp/sot_extra
	$(CXX) $(CXXFLAGS) -Wno-misleading-indentation tests/test_ooo.cpp -o /tmp/sot_ooo -lpthread && /tmp/sot_ooo 150
	$(CXX) $(CXXFLAGS) tests/test_mem.cpp -o /tmp/sot_mem -lpthread && /tmp/sot_mem
sanitize:
	$(CXX) -std=c++17 -O1 -g -march=native -fsanitize=address,undefined -fno-sanitize-recover=all tests/test.cpp -o /tmp/sot_asan -lpthread && /tmp/sot_asan 2000 200
	$(CXX) -std=c++17 -O1 -g -fsanitize=thread tests/test.cpp -o /tmp/sot_tsan -lpthread && /tmp/sot_tsan 2000 80
	$(CXX) -std=c++17 -O1 -g -fsanitize=thread -Wno-misleading-indentation tests/test_ooo.cpp -o /tmp/sot_ooo_tsan -lpthread && /tmp/sot_ooo_tsan 60
adversary:
	$(CXX) -std=c++17 -O2 -march=native tests/adv_quick.cpp -o /tmp/sot_adv && /tmp/sot_adv
	$(CXX) -std=c++17 -O2 -march=native -DSOT_V6_BUG tests/adv_quick.cpp -o /tmp/sot_adv6 && /tmp/sot_adv6
.PHONY: test sanitize adversary
