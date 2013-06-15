#include "utils.h"
#include "ioengine.h"

struct ioengine_ops *ioengine;

void register_ioengine(struct ioengine_ops *ops)
{
	ioengine = ops;
}
