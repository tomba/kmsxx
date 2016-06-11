#include <string.h>
#include <iostream>
#include <stdexcept>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

DrmObject::DrmObject(Card& card, uint32_t object_type)
	:m_card(card), m_id(-1), m_object_type(object_type), m_idx(0)
{
}

DrmObject::DrmObject(Card& card, uint32_t id, uint32_t object_type, uint32_t idx)
	:m_card(card), m_id(id), m_object_type(object_type), m_idx(idx)
{
}

DrmObject::~DrmObject()
{

}

void DrmObject::set_id(uint32_t id)
{
	m_id = id;
}
}
