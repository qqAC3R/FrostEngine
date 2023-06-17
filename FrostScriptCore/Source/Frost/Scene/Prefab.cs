using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Frost
{
    public class Prefab
    {
        public ulong ID { get; private set; }

        protected Prefab() { ID = 0; }

        internal Prefab(ulong id)
        {
            ID = id;
        }
    }
}
