using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Security.Policy;
using System.Text;
using System.Threading.Tasks;

namespace Frost
{
    public abstract class Component
    {
        public Entity Entity { get; set; }
    }

    public class TagComponent : Component
    {
        public string Tag
        {
            get
            {
                return GetTag_Native(Entity.ID);
            }

            set
            {
                SetTag_Native(Entity.ID, value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern string GetTag_Native(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetTag_Native(ulong entityID, string tag);
    }

    public class TransformComponent : Component
    {
        public Transform Transform
        {
            get
            {
                GetTransform_Native(Entity.ID, out Transform result);
                return result;
            }

            set
            {
                SetTransform_Native(Entity.ID, ref value);
            }
        }

        public Transform WorldTransform
        {
            get
            {
                GetWorldSpaceTransform_Native(Entity.ID, out Transform result);
                return result;
            }
        }

        public Vector3 Translation
        {
            get
            {
                GetTranslation_Native(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                SetTranslation_Native(Entity.ID, ref value);
            }
        }

        public Vector3 Rotation
        {
            get
            {
                GetRotation_Native(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                SetRotation_Native(Entity.ID, ref value);
            }
        }

        public Vector3 Scale
        {
            get
            {
                GetScale_Native(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                SetScale_Native(Entity.ID, ref value);
            }
        }

        // For Transform
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetTransform_Native(ulong entityID, out Transform outTransform);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTransform_Native(ulong entityID, ref Transform inTransform);

        // For Translation
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetTranslation_Native(ulong entityID, out Vector3 outTranslation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTranslation_Native(ulong entityID, ref Vector3 inTranslation);

        // For Rotation
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRotation_Native(ulong entityID, out Vector3 outRotation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRotation_Native(ulong entityID, ref Vector3 inRotation);

        // For Scale
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetScale_Native(ulong entityID, out Vector3 outScale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScale_Native(ulong entityID, ref Vector3 inScale);


        // For Global Transform (from parent)
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldSpaceTransform_Native(ulong entityID, out Transform outTransform);
    }

    public class MeshComponent : Component
    {
        public Mesh Mesh
        {
            get
            {
                Mesh result = new Mesh(GetMesh_Native(Entity.ID));
                return result;
            }

            set
            {
                IntPtr ptr = value == null ? IntPtr.Zero : value.m_UnmanagedInstance;
                SetMesh_Native(Entity.ID, ptr);
            }
        }

        public bool HasMaterial(int index)
        {
            return HasMaterial_Native(Entity.ID, index);
        }

        public Material GetMaterial(int index)
        {
            if (!HasMaterial(index))
                return null;

            return new Material(GetMaterial_Native(Entity.ID, index));
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetMesh_Native(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMesh_Native(ulong entityID, IntPtr unmanagedInstance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasMaterial_Native(ulong entityID, int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetMaterial_Native(ulong entityID, int index);
    }

    //  TODO: This is barebones, it must be redone from the ground up!
    public class AnimationComponent : Component
    {
        public string[] Animations
        {
            get => GetAnimations_Native(Entity.ID);
        }

        public void SetFloatInput(string inputName, float input)
        {
            SetFloatInput_Native(Entity.ID, inputName, input);
        }

        public void SetIntInput(string inputName, int input)
        {
            SetIntInput_Native(Entity.ID, inputName, input);
        }

        public void SetBoolInput(string inputName, bool input)
        {
            SetBoolInput_Native(Entity.ID, inputName, input);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string[] GetAnimations_Native(ulong entityID);


        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFloatInput_Native(ulong entityID, string inputName, float input);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIntInput_Native(ulong entityID, string inputName, int input);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetBoolInput_Native(ulong entityID, string inputName, bool input);
    }

    public class CameraComponent : Component
    {
        public float FOV
        { 
            get => GetFOV_Native(Entity.ID);
            set => SetFOV_Native(Entity.ID, value);
        }

        public float NearClip
        {
            get => GetNearClip_Native(Entity.ID);
            set => SetNearClip_Native(Entity.ID, value);
        }

        public float FarClip
        {
            get => GetFarClip_Native(Entity.ID);
            set => SetFarClip_Native(Entity.ID, value);
        }

        // FOV
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetFOV_Native(ulong entityID, float fov);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float GetFOV_Native(ulong entityID);

        // Near Clip
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetNearClip_Native(ulong entityID, float nearClip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float GetNearClip_Native(ulong entityID);

        // Far Clip
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetFarClip_Native(ulong entityID, float farClip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float GetFarClip_Native(ulong entityID);

    }

    public class ScriptComponent : Component
    {
        public object Instance
        {
            get => GetInstance_Native(Entity.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern object GetInstance_Native(ulong entityID);
    }

    public class RigidBodyComponent : Component
    {
        public enum Type
        {
            Static,
            Dynamic
        }

        public Type BodyType
        {
            get
            {
                return GetBodyType_Native(Entity.ID);
            }
        }

        public Vector3 Translation
        {
            get
            {
                GetTranslation_Native(Entity.ID, out Vector3 translation);
                return translation;
            }

            set
            {
                SetTranslation_Native(Entity.ID, ref value);
            }
        }

        public Vector3 Rotation
        {
            get
            {
                GetRotation_Native(Entity.ID, out Vector3 rotation);
                return rotation;
            }

            set
            {
                SetRotation_Native(Entity.ID, ref value);
            }
        }

        public float Mass
        {
            get { return GetMass_Native(Entity.ID); }
            set { SetMass_Native(Entity.ID, value); }
        }

        public Vector3 LinearVelocity
        {
            get
            {
                GetLinearVelocity_Native(Entity.ID, out Vector3 velocity);
                return velocity;
            }

            set { SetLinearVelocity_Native(Entity.ID, ref value); }
        }

        public Vector3 AngularVelocity
        {
            get
            {
                GetAngularVelocity_Native(Entity.ID, out Vector3 velocity);
                return velocity;
            }

            set { SetAngularVelocity_Native(Entity.ID, ref value); }
        }

        public float MaxLinearVelocity
        {
            get { return GetMaxLinearVelocity_Native(Entity.ID); }
            set { SetMaxLinearVelocity_Native(Entity.ID, value); }
        }

        public float MaxAngularVelocity
        {
            get { return GetMaxAngularVelocity_Native(Entity.ID); }
            set { SetMaxAngularVelocity_Native(Entity.ID, value); }
        }

        // TODO: When I'll add physics layers, I must also add support to manage them from C# script
        public uint Layer
        {
            get => GetLayer_Native(Entity.ID);
        }

        public void GetKinematicTarget(out Vector3 targetPosition, out Vector3 targetRotation)
        {
            GetKinematicTarget_Native(Entity.ID, out targetPosition, out targetRotation);
        }

        public void SetKinematicTarget(Vector3 targetPosition, Vector3 targetRotation)
        {
            SetKinematicTarget_Native(Entity.ID, ref targetPosition, ref targetRotation);
        }

        public void AddForce(Vector3 force, ForceMode forceMode = ForceMode.Force)
        {
            AddForce_Native(Entity.ID, ref force, forceMode);
        }

        public void AddTorque(Vector3 torque, ForceMode forceMode = ForceMode.Force)
        {
            AddTorque_Native(Entity.ID, ref torque, forceMode);
        }

        // Rotation should be in radians
        public void Rotate(Vector3 rotation)
        {
            Rotate_Native(Entity.ID, ref rotation);
        }

        public void SetLockFlag(ActorLockFlag flag, bool value) => SetLockFlag_Native(Entity.ID, flag, value);
        public bool IsLockFlagSet(ActorLockFlag flag) => IsLockFlagSet_Native(Entity.ID, flag);
        public uint GetLockFlags() => GetLockFlags_Native(Entity.ID);


        // Body Type
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Type GetBodyType_Native(ulong entityID);

        // Position
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetTranslation_Native(ulong entityID, out Vector3 translation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTranslation_Native(ulong entityID, ref Vector3 translation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRotation_Native(ulong entityID, out Vector3 rotation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRotation_Native(ulong entityID, ref Vector3 rotation);

        // Mass
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMass_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMass_Native(ulong entityID, float mass);

        // Velocity
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetLinearVelocity_Native(ulong entityID, out Vector3 velocity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLinearVelocity_Native(ulong entityID, ref Vector3 velocity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAngularVelocity_Native(ulong entityID, out Vector3 velocity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAngularVelocity_Native(ulong entityID, ref Vector3 velocity);

        // Max Velocity
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxLinearVelocity_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxLinearVelocity_Native(ulong entityID, float maxVelocity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxAngularVelocity_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxAngularVelocity_Native(ulong entityID, float maxVelocity);

        // Kinematics
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetKinematicTarget_Native(ulong entityID, out Vector3 targetPosition, out Vector3 targetRotation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetKinematicTarget_Native(ulong entityID, ref Vector3 targetPosition, ref Vector3 targetRotation);


        // Addition of Torque/Force
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddForce_Native(ulong entityID, ref Vector3 force, ForceMode forceMode);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddTorque_Native(ulong entityID, ref Vector3 torque, ForceMode forceMode);

        // Rotate
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Rotate_Native(ulong entityID, ref Vector3 rotation);

        // Lock Flags
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLockFlag_Native(ulong entityID, ActorLockFlag flag, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsLockFlagSet_Native(ulong entityID, ActorLockFlag flag);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetLockFlags_Native(ulong entityID);

        // Layers
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetLayer_Native(ulong entityID);
    }

    public class BoxColliderComponent : Component
    {
        public Vector3 Size
        {
            get
            {
                GetSize_Native(Entity.ID, out Vector3 size);
                return size;
            }
            set => SetSize_Native(Entity.ID, ref value);
        }

        public Vector3 Offset
        {
            get
            {
                GetOffset_Native(Entity.ID, out Vector3 offset);
                return offset;
            }
            set => SetOffset_Native(Entity.ID, ref value);
        }

        public bool IsTrigger
        {
            get => GetIsTrigger_Native(Entity.ID);
            set => SetIsTrigger_Native(Entity.ID, value);
        }

        // Size
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetSize_Native(ulong entityID, out Vector3 size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSize_Native(ulong entityID, ref Vector3 size);

        // Offset
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetOffset_Native(ulong entityID, out Vector3 offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOffset_Native(ulong entityID, ref Vector3 offset);

        // Trigger
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsTrigger_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsTrigger_Native(ulong entityID, bool isTrigger);
    }

    public class SphereColliderComponent : Component
    {
        public float Radius
        {
            get => GetRadius_Native(Entity.ID);
            set => SetRadius_Native(Entity.ID, value);
        }

        public Vector3 Offset
        {
            get
            {
                GetOffset_Native(Entity.ID, out Vector3 offset);
                return offset;
            }
            set => SetOffset_Native(Entity.ID, ref value);
        }

        public bool IsTrigger
        {
            get => GetIsTrigger_Native(Entity.ID);
            set => SetIsTrigger_Native(Entity.ID, value);
        }

        // Radius
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetRadius_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRadius_Native(ulong entityID, float radius);

        // Offset
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetOffset_Native(ulong entityID, out Vector3 offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOffset_Native(ulong entityID, ref Vector3 offset);

        // Trigger
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsTrigger_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsTrigger_Native(ulong entityID, bool isTrigger);
    }

    public class CapsuleColliderComponent : Component
    {
        public float Radius
        {
            get => GetRadius_Native(Entity.ID);
            set => SetRadius_Native(Entity.ID, value);
        }

        public float Height
        {
            get => GetHeight_Native(Entity.ID);
            set => SetHeight_Native(Entity.ID, value);
        }

        public Vector3 Offset
        {
            get
            {
                GetOffset_Native(Entity.ID, out Vector3 offset);
                return offset;
            }
            set => SetOffset_Native(Entity.ID, ref value);
        }

        public bool IsTrigger
        {
            get => GetIsTrigger_Native(Entity.ID);
            set => SetIsTrigger_Native(Entity.ID, value);
        }


        // Height
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetHeight_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetHeight_Native(ulong entityID, float height);

        // Radius
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetRadius_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRadius_Native(ulong entityID, float radius);

        // Offset
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetOffset_Native(ulong entityID, out Vector3 offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOffset_Native(ulong entityID, ref Vector3 offset);

        // Trigger
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsTrigger_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsTrigger_Native(ulong entityID, bool isTrigger);
    }

    public class MeshColliderComponent : Component
    {
        public Mesh ColliderMesh
        {
            get
            {
                Mesh result = new Mesh(GetColliderMesh_Native(Entity.ID));
                return result;
            }
            // TODO: Add setter
            /*set
			{
				IntPtr ptr = value == null ? IntPtr.Zero : value.m_UnmanagedInstance;
				SetColliderMesh_Native(Entity.ID, ptr);
			}*/
        }

        public bool IsConvex
        {
            get => IsConvex_Native(Entity.ID);
            set => SetConvex_Native(Entity.ID, value);
        }

        public bool IsTrigger
        {
            get => GetIsTrigger_Native(Entity.ID);
            set => SetIsTrigger_Native(Entity.ID, value);
        }


        // Collider Mesh
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetColliderMesh_Native(ulong entityID);

        // Convex
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsConvex_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetConvex_Native(ulong entityID, bool isConvex);

        // Trigger
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsTrigger_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsTrigger_Native(ulong entityID, bool isTrigger);
    }
}
