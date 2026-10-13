/*
 * ES.h
 *
 *  Created on: 13 Jan 2021
 *      Author: pgm
 */

#ifndef ES_H_
#define ES_H_

#define SOB_MAGIC 0xaa55fbff
#define EVX_MAGIC 0xaa55f150
#define EVX_MASK 0xfffffff0
#define EV0_MAGIC 0xaa55f151
#define EV1_MAGIC 0xaa55f152

bool ISACQ480();

#define IX_SC (ISACQ480() ? 0 : 4)
#define IX_SCC (ISACQ480() ? 2 : 5)

class AbstractES {
    public:
	virtual bool isES(unsigned *cursor) = 0;

	const int ix_sc;
	const int ix_scc;
	AbstractES()
		: ix_sc(IX_SC)
		, ix_scc(IX_SCC)
	{
	}

	static AbstractES *evX_instance();
	static AbstractES *ev0_instance();
	static AbstractES *ev1_instance();
};

#endif /* ES_H_ */
