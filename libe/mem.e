use unix -> PAGE_SIZE, PROT_READ, PROT_WRITE, MAP_PRIVATE, MAP_ANONYMOUS

struct RegionHeader
  size: u64,
  head: u64,
  first_segment: &SegmentHeader,
  next: &RegionHeader,
end

struct SegmentHeader
  size: u64,
  is_consumed: u64,
  next: &SegmentHeader,
  __pad0: u64,
end

const PAGES_PER_REGION: u64 = 10

static first_region: &RegionHeader

proc align(value: u64, base: u64) -> u64
  if value % base == 0 as u64
    retval value
  end
  retval value + base - value % base
end

proc alloc(size: u64) -> &u8
  if size == 0 as u64
    retval null
  end

  let real_size = sizeof SegmentHeader + size

  let region_ptr = &first_region
  let region = first_region

  while region != null
    let segment_ptr = &region.first_segment
    let segment = region.first_segment
    let aligned_size = align(size, 16 as u64)
    let not_found = true

    while not_found & (segment != null)
      if segment.is_consumed |
         (segment.size < size) as u64 |
         ((segment.size != size) as u64 &
          (segment.size < aligned_size + sizeof SegmentHeader + 8 as u64) as u64) as u64
        segment_ptr = &segment.next
        segment = segment.next
      else
        not_found = false
      end
    end

    if segment == null
      if region.head + real_size <= region.size
        let result = region as &u8 + sizeof RegionHeader + region.head
        region.head = region.head + align(real_size, 16 as u64)

        segment = result as &SegmentHeader
        segment.size = size
        segment.is_consumed = 1 as u64
        segment.next = null

        segment_ptr := segment

        retval result + sizeof SegmentHeader
      end
    else
      if segment.size >= aligned_size + sizeof SegmentHeader + 8 as u64
        let next = segment.next

        segment.next =
          (segment as &u8 + aligned_size + sizeof SegmentHeader) as &SegmentHeader
        segment.next.size = segment.size - aligned_size - sizeof SegmentHeader
        segment.next.is_consumed = 0 as u64
        segment.next.next = next

        segment.size = size
      end

      segment.is_consumed = 1 as u64

      retval segment as &u8 + sizeof SegmentHeader
    end

    region_ptr = &region.next
    region = region.next
  end

  let region_size = PAGE_SIZE * PAGES_PER_REGION
  if region_size < sizeof RegionHeader + real_size
    region_size = align(sizeof RegionHeader + real_size, PAGE_SIZE)
  end

  region =
    unix::mmap(0 as u64, region_size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 as u64) as &RegionHeader
  if region as s64 < 0
    retval null
  end

  region.size = region_size - sizeof RegionHeader
  region.head = 0 as u64
  region.first_segment = null
  region.next = null

  region_ptr := region

  let result = region as &u8 + sizeof RegionHeader + region.head
  region.head = region.head + align(real_size, 16 as u64)

  let segment = result as &SegmentHeader
  segment.size = size
  segment.is_consumed = 1 as u64
  segment.next = null

  region.first_segment = segment

  retval result + sizeof SegmentHeader
end

proc free(ptr: &u8)
  let segment = (ptr - sizeof SegmentHeader) as &SegmentHeader
  segment.is_consumed = 0 as u64
end
