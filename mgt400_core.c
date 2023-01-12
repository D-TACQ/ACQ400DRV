
#include "acq400.h"
#include "mgt400.h"


#undef DEVP
#define DEVP(mdev)		(&(mdev)->pdev->dev)



void mgt400wr32(struct mgt400_dev *mdev, int offset, u32 value)
{
	if (mdev->RW32_debug){
		dev_info(DEVP(mdev), "mgt400wr32 %p [0x%02x] = %08x\n",
				mdev->va + offset, offset, value);
	}else{
		dev_dbg(DEVP(mdev), "mgt400wr32 %p [0x%02x] = %08x\n",
				mdev->va + offset, offset, value);
	}

	iowrite32(value, mdev->va + offset);
}

u32 mgt400rd32(struct mgt400_dev *mdev, int offset)
{
	u32 rc = ioread32(mdev->va + offset);
	if (mdev->RW32_debug){
		dev_info(DEVP(mdev), "mgt400rd32 %p [0x%02x] = %08x\n",
			mdev->va + offset, offset, rc);
	}else{
		dev_dbg(DEVP(mdev), "mgt400rd32 %p [0x%02x] = %08x\n",
			mdev->va + offset, offset, rc);
	}
	return rc;
}

